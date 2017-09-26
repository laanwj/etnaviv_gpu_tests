/*
 * Copyright (C) 2017 Etnaviv Project.
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
/* Basic triangle rendering test */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "drm_setup.h"
#include "cmdstream.h"
#include "write_bmp.h"

#include "state.xml.h"
#include "state_3d.xml.h"
#include "common.xml.h"

/* TODO:
 * - process output (un-supertile resolve)
 * - free/cleanup
 */

/* Data stream */
static const float vData[] = {
         0.0f,  1.0f, 0.0f, 1.0f, // point 1
         1.0f,  0.0f, 0.0f, 1.0f, // red
        -1.0f, -1.0f, 0.0f, 1.0f, // point 2
         0.0f,  1.0f, 0.0f, 1.0f, // green
         1.0f, -1.0f, 0.0f, 1.0f, // point 3
         0.0f,  0.0f, 1.0f, 1.0f, // blue
};

struct attachments {
    struct etna_bo *color_status;
    struct etna_bo *color_surface;
    struct etna_bo *depth_status;
    struct etna_bo *depth_surface;
    struct etna_bo *stream_data;

    struct etna_reloc color_status_base;
    struct etna_reloc color_surface_base;
    struct etna_reloc depth_status_base;
    struct etna_reloc depth_surface_base;
    struct etna_reloc stream_base;
};

/* 1280x800 */
static const size_t color_status_size = 0x4400;
static const size_t color_surface_size = 0x410000;
static const size_t depth_status_size = 0x4400;
static const size_t depth_surface_size = 0x410000;

static bool initialize_attachments(struct etna_device *dev, struct attachments *at)
{
    /* Allocate memory for depth and surface attachments */
    at->color_status = etna_bo_new(dev, color_status_size, DRM_ETNA_GEM_CACHE_UNCACHED);
    at->color_surface = etna_bo_new(dev, color_surface_size, DRM_ETNA_GEM_CACHE_UNCACHED);
    at->depth_status = etna_bo_new(dev, depth_status_size, DRM_ETNA_GEM_CACHE_UNCACHED);
    at->depth_surface = etna_bo_new(dev, depth_surface_size, DRM_ETNA_GEM_CACHE_UNCACHED);
    at->stream_data = etna_bo_new(dev, sizeof(vData), DRM_ETNA_GEM_CACHE_UNCACHED);

    if (!at->color_status || !at->color_surface ||
        !at->depth_status || !at->depth_surface || !at->stream_data) {
        fprintf(stderr, "Error allocating memory\n");
        return false;
    }

    /* Set base bo's */
    at->color_status_base.bo = at->color_status;
    at->color_status_base.flags = ETNA_RELOC_WRITE | ETNA_RELOC_READ;
    at->color_status_base.offset = 0;

    at->color_surface_base.bo = at->color_surface;
    at->color_surface_base.flags = ETNA_RELOC_WRITE | ETNA_RELOC_READ;
    at->color_surface_base.offset = 0;

    at->depth_status_base.bo = at->depth_status;
    at->depth_status_base.flags = ETNA_RELOC_WRITE | ETNA_RELOC_READ;
    at->depth_status_base.offset = 0;

    at->depth_surface_base.bo = at->depth_surface;
    at->depth_surface_base.flags = ETNA_RELOC_WRITE | ETNA_RELOC_READ;
    at->depth_surface_base.offset = 0;

    at->stream_base.bo = at->stream_data;
    at->stream_base.flags = ETNA_RELOC_READ;
    at->stream_base.offset = 0;

    /* Clear surfaces */
    memset(etna_bo_map(at->color_status), 0, color_status_size);
    memset(etna_bo_map(at->color_surface), 0, color_surface_size);
    memset(etna_bo_map(at->depth_status), 0, depth_status_size);
    memset(etna_bo_map(at->depth_surface), 0, depth_surface_size);

    /* Upload stream data */
    unsigned char *mapped = etna_bo_map(at->stream_data);
    memcpy(mapped, vData, sizeof(vData));
    return true;
}

static bool free_attachments(struct etna_device *dev, struct attachments *at)
{
    return true;
}

/* render frame from blob */
static void build_frame_cmdbuf(struct etna_cmd_stream *stream, struct attachments *at)
{
    etna_set_state(stream, VIVS_GL_VERTEX_ELEMENT_CONFIG, 0x00000001);
    etna_set_state(stream, VIVS_RA_CONTROL, 0x00000001);
    etna_set_state(stream, VIVS_PA_W_CLIP_LIMIT, 0x34000001);
    etna_set_state(stream, VIVS_PE_LOGIC_OP, 0xfffffbff);
    etna_set_state(stream, VIVS_RA_UNK00E0C, 0x00000000);
    etna_set_state(stream, VIVS_PA_FLAGS, 0x40000000);
    etna_set_state(stream, VIVS_PA_SYSTEM_MODE, 0x00000011);
    etna_set_state(stream, VIVS_GL_API_MODE, 0x00000000);
    etna_set_state(stream, VIVS_SE_CONFIG, 0x00000000);

    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000003);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000003);
    etna_set_state(stream, VIVS_TS_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000003);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000003);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000003);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000003);
    etna_set_state(stream, VIVS_TS_FLUSH_CACHE, 0x00000001);

    etna_stall(stream, SYNC_RECIPIENT_RA, SYNC_RECIPIENT_PE);
    etna_set_state(stream, VIVS_RS_CONFIG, 0x00000606);
    etna_set_state(stream, VIVS_RS_DITHER(0), 0xffffffff);
    etna_set_state(stream, VIVS_RS_DITHER(1), 0xffffffff);
    etna_set_state_reloc(stream, VIVS_RS_DEST_ADDR, &at->color_status_base);
    etna_set_state_reloc(stream, VIVS_RS_PIPE_DEST_ADDR(0), &at->color_status_base);
    etna_set_state(stream, VIVS_RS_SOURCE_STRIDE, 0x00000000);
    etna_set_state(stream, VIVS_RS_DEST_STRIDE, 0x00000080);
    etna_set_state(stream, VIVS_RS_FILL_VALUE(0), 0x55555555);
    etna_set_state(stream, VIVS_RS_CLEAR_CONTROL, 0x0001ffff);
    etna_set_state(stream, VIVS_RS_EXTRA_CONFIG, 0x00100000);
    etna_set_state(stream, VIVS_RS_WINDOW_SIZE, 0x00880020);
    etna_set_state(stream, VIVS_RS_PIPE_OFFSET(0), 0x00000000);
    etna_set_state(stream, VIVS_RS_SINGLE_BUFFER, 0x00000001);
    etna_set_state(stream, VIVS_RS_KICKER, 0xbadabeeb);
    etna_set_state(stream, VIVS_RS_SINGLE_BUFFER, 0x00000000);
    etna_stall(stream, SYNC_RECIPIENT_RA, SYNC_RECIPIENT_PE);

    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000002);
    etna_set_state(stream, VIVS_DUMMY_DUMMY, 0x00000000);
    etna_set_state(stream, VIVS_TS_COLOR_AUTO_DISABLE_COUNT, 0x00010400);
    etna_set_state_reloc(stream, VIVS_TS_COLOR_STATUS_BASE, &at->color_status_base);
    etna_set_state_reloc(stream, VIVS_TS_COLOR_SURFACE_BASE, &at->color_surface_base);
    etna_set_state(stream, VIVS_TS_COLOR_CLEAR_VALUE, 0xff808080);
    etna_set_state(stream, VIVS_RS_UNK016BC, 0xff808080);
    etna_set_state(stream, VIVS_TS_MEM_CONFIG, 0x00000022);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_DUMMY_DUMMY, 0x00000000);
    etna_set_state_reloc(stream, VIVS_TS_DEPTH_STATUS_BASE, &at->depth_status_base);
    etna_set_state_reloc(stream, VIVS_TS_DEPTH_SURFACE_BASE, &at->depth_surface_base);
    etna_set_state(stream, VIVS_TS_DEPTH_CLEAR_VALUE, 0xffffff00);
    etna_set_state(stream, VIVS_TS_MEM_CONFIG, 0x00000063);

    etna_set_state(stream, VIVS_SE_DEPTH_SCALE, 0x00000000);
    etna_set_state(stream, VIVS_SE_DEPTH_BIAS, 0x00000000);
    etna_set_state(stream, VIVS_FE_VERTEX_ELEMENT_CONFIG(0), 0x10000008);
    etna_set_state(stream, VIVS_FE_VERTEX_ELEMENT_CONFIG(1), 0x20100088);
    etna_set_state(stream, VIVS_VS_INPUT(0), 0x00000100);
    etna_set_state_reloc(stream, VIVS_NFE_VERTEX_STREAMS_BASE_ADDR(0), &at->stream_base);
    etna_set_state(stream, VIVS_NFE_VERTEX_STREAMS_CONTROL(0), 0x00000020);
    etna_set_state(stream, VIVS_NFE_VERTEX_STREAMS_UNK14680(0), 0x00000000);
    etna_set_state(stream, VIVS_FE_UNK00780(0), 0x3f800000);
    etna_set_state(stream, VIVS_FE_UNK00780(1), 0x3f800000);
    // How to emit 0 address?
    //etna_set_state_reloc(stream, VIVS_FE_INDEX_STREAM_BASE_ADDR, 0x00000000);
    etna_set_state(stream, VIVS_FE_INDEX_STREAM_CONTROL, 0x00000000);
    etna_set_state(stream, VIVS_FE_PRIMITIVE_RESTART_INDEX, 0x00000000);
    etna_set_state_fixp(stream, VIVS_PA_VIEWPORT_SCALE_X, 0x02800000);
    etna_set_state_fixp(stream, VIVS_PA_VIEWPORT_SCALE_Y, 0xfe700000);
    etna_set_state_fixp(stream, VIVS_PA_VIEWPORT_OFFSET_X, 0x02800000);
    etna_set_state_fixp(stream, VIVS_PA_VIEWPORT_OFFSET_Y, 0x01900000);
    etna_set_state(stream, VIVS_PA_VIEWPORT_UNK00A80, 0x38a01404);
    etna_set_state_fixp(stream, VIVS_PA_VIEWPORT_UNK00A84, 0x20000000);
    etna_set_state(stream, VIVS_PA_ZFARCLIPPING, 0x00000000);
    etna_set_state_fixp(stream, VIVS_SE_SCISSOR_LEFT, 0x00000000);
    etna_set_state_fixp(stream, VIVS_SE_SCISSOR_TOP, 0x00000000);
    etna_set_state_fixp(stream, VIVS_SE_SCISSOR_RIGHT, 0x05001119);
    etna_set_state_fixp(stream, VIVS_SE_SCISSOR_BOTTOM, 0x03201111);
    etna_set_state_fixp(stream, VIVS_SE_CLIP_RIGHT, 0x0500ffff);
    etna_set_state_fixp(stream, VIVS_SE_CLIP_BOTTOM, 0x0320ffff);
    etna_set_state(stream, VIVS_PE_ALPHA_OP, 0x00000070);
    etna_set_state(stream, VIVS_PE_ALPHA_BLEND_COLOR, 0x00000000);
    etna_set_state(stream, VIVS_PE_ALPHA_CONFIG, 0x00100010);
    etna_set_state(stream, VIVS_PE_ALPHA_COLOR_EXT0, 0x00000000);
    etna_set_state(stream, VIVS_PE_ALPHA_COLOR_EXT1, 0x00000000);
    etna_set_state(stream, VIVS_PE_STENCIL_CONFIG_EXT, 0x0000fdff);
    etna_set_state(stream, VIVS_PE_LOGIC_OP, 0xfffffbff);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000002);
    etna_set_state(stream, VIVS_PE_COLOR_FORMAT, 0x06110f10);
    etna_set_state(stream, VIVS_PE_DITHER(0), 0x6e4ca280);
    etna_set_state(stream, VIVS_PE_DITHER(1), 0x5d7f91b3);
    etna_set_state_reloc(stream, VIVS_PE_PIPE_COLOR_ADDR(0), &at->color_surface_base);
    etna_set_state_reloc(stream, VIVS_PE_PIPE_COLOR_ADDR(1), &at->color_surface_base);
    etna_set_state(stream, VIVS_PE_COLOR_STRIDE, 0x00001400);
    etna_set_state(stream, VIVS_PE_LOGIC_OP, 0xfffffe7f);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_PE_DEPTH_CONFIG, 0x05050710);
    etna_set_state(stream, VIVS_PE_DEPTH_NEAR, 0x00000000);
    etna_set_state(stream, VIVS_PE_DEPTH_FAR, 0x3f800000);
    etna_set_state(stream, VIVS_PE_DEPTH_NORMALIZE, 0x4b7fffff);
    etna_set_state_reloc(stream, VIVS_PE_PIPE_DEPTH_ADDR(0), &at->depth_surface_base);
    etna_set_state_reloc(stream, VIVS_PE_PIPE_DEPTH_ADDR(1), &at->depth_surface_base);
    etna_set_state(stream, VIVS_PE_LOGIC_OP, 0xfffffe7f);
    etna_set_state(stream, VIVS_PE_DEPTH_STRIDE, 0x00001400);
    etna_set_state(stream, VIVS_PE_LOGIC_OP, 0xff4fffff);
    etna_set_state(stream, VIVS_RA_EARLY_DEPTH, 0x00000031);
    etna_set_state(stream, VIVS_PE_HDEPTH_CONTROL, 0x00000000);
    etna_set_state(stream, VIVS_RA_HDEPTH_CONTROL, 0x00007000);
    etna_set_state(stream, VIVS_PA_VIEWPORT_SCALE_Z, 0x3f000000);
    etna_set_state(stream, VIVS_PA_VIEWPORT_OFFSET_Z, 0x3f000000);
    etna_set_state(stream, VIVS_PE_STENCIL_OP, 0x00070007);
    etna_set_state(stream, VIVS_PE_STENCIL_CONFIG, 0x00000000);
    etna_set_state(stream, VIVS_PE_STENCIL_CONFIG_EXT, 0xfffffe00);
    etna_set_state(stream, VIVS_PE_STENCIL_CONFIG_EXT2, 0x00000000);
    etna_set_state(stream, VIVS_GL_MULTI_SAMPLE_CONFIG, 0x00000000);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000003);

    etna_stall(stream, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);

    etna_set_state(stream, VIVS_PE_LOGIC_OP, 0xf0ffffff);
    etna_set_state(stream, VIVS_PA_LINE_WIDTH, 0x3f000000);
    etna_set_state(stream, VIVS_PA_CONFIG, 0x00412100);
    etna_set_state(stream, VIVS_PA_WIDE_LINE_WIDTH0, 0x3f000000);
    etna_set_state(stream, VIVS_PA_WIDE_LINE_WIDTH1, 0x3f000000);

    etna_stall(stream, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);

    etna_set_state(stream, VIVS_VS_INPUT_COUNT, 0x00000102);
    etna_set_state(stream, VIVS_VS_TEMP_REGISTER_CONTROL, 0x00000002);
    etna_set_state(stream, VIVS_VS_OUTPUT(0), 0x00000100);
    etna_set_state(stream, VIVS_VS_UNIFORM_CACHE, 0x00000000);
    etna_set_state(stream, VIVS_VS_RANGE, 0x00000000);
    etna_set_state(stream, VIVS_VS_ICACHE_CONTROL, 0x00000010);
    //etna_set_state(stream, VIVS_VS_INST_ADDR, 0x00000000);
    etna_set_state(stream, VIVS_VS_UNIFORM_BASE, 0x00000000);
    etna_set_state(stream, VIVS_SH_INST_MEM(0), 0x00000000);
    etna_set_state(stream, VIVS_SH_INST_MEM(1), 0x00000000);
    etna_set_state(stream, VIVS_SH_INST_MEM(2), 0x00000000);
    etna_set_state(stream, VIVS_SH_INST_MEM(3), 0x00000000);
    etna_set_state(stream, VIVS_RA_CONTROL, 0x00000001);
    etna_set_state(stream, VIVS_PS_OUTPUT_REG, 0x00000001);
    etna_set_state(stream, VIVS_PS_TEMP_REGISTER_CONTROL, 0x00000002);
    etna_set_state(stream, VIVS_PA_SHADER_ATTRIBUTES(0), 0x00000200);
    etna_set_state(stream, VIVS_GL_VARYING_NUM_COMPONENTS, 0x00000004);
    etna_set_state(stream, VIVS_GL_UNK03834, 0x00000000);
    etna_set_state(stream, VIVS_GL_VARYING_COMPONENT_USE(0), 0x00000000);
    etna_set_state(stream, VIVS_GL_VARYING_COMPONENT_USE(1), 0x00000000);
    etna_set_state(stream, VIVS_GL_UNK03838, 0x00000000);
    etna_set_state(stream, VIVS_GL_UNK03854, 0x00000000);
    etna_set_state(stream, VIVS_VS_UNIFORM_CACHE, 0x00000011);
    etna_set_state(stream, VIVS_PS_RANGE, 0x01ff01ff);
    etna_set_state(stream, VIVS_VS_ICACHE_CONTROL, 0x00000020);
    //etna_set_state(stream, VIVS_PS_INST_ADDR, 0x00000000);
    etna_set_state(stream, VIVS_PS_UNIFORM_BASE, 0x0000013f);
    etna_set_state(stream, VIVS_SH_INST_MEM_MIRROR(2044), 0x00000000);
    etna_set_state(stream, VIVS_SH_INST_MEM_MIRROR(2045), 0x00000000);
    etna_set_state(stream, VIVS_SH_INST_MEM_MIRROR(2046), 0x00000000);
    etna_set_state(stream, VIVS_SH_INST_MEM_MIRROR(2047), 0x00000000);
    etna_set_state(stream, VIVS_PS_INPUT_COUNT, 0x00001f02);
    etna_set_state(stream, VIVS_PS_TEMP_REGISTER_CONTROL, 0x00000002);
    etna_set_state(stream, VIVS_PS_CONTROL, 0x00000002);
    etna_set_state(stream, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT, 0x00000100);
    etna_set_state(stream, VIVS_GL_VARYING_TOTAL_COMPONENTS, 0x00000004);
    etna_set_state(stream, VIVS_PS_CONTROL_EXT, 0x00000000);
    etna_set_state(stream, VIVS_VS_LOAD_BALANCING, 0x0f3f022a);
    etna_set_state(stream, VIVS_VS_OUTPUT_COUNT, 0x00000002);

    etna_stall(stream, SYNC_RECIPIENT_RA, SYNC_RECIPIENT_PE);

    etna_set_state(stream, VIVS_GL_VERTEX_ELEMENT_CONFIG, 0x00000011);

    /* command DRAW_INSTANCED (12) OP=DRAW_INSTANCED,INDEXED=0,TYPE=TRIANGLE_STRIP,INSTANCE_COUNT_LO=0x1 */
    /* command   COUNT INSTANCE_COUNT_HI=0x0,VERTEX_COUNT=0x3 */
    /* command   START INDEX=0x0 */
    /* command PAD */
    etna_cmd_stream_reserve(stream, 4);
    etna_cmd_stream_emit(stream, 0x60050001);
    etna_cmd_stream_emit(stream, 0x00000003);
    etna_cmd_stream_emit(stream, 0x00000000);
    etna_cmd_stream_emit(stream, 0x00000000);

    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000020);

    etna_stall(stream, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);

    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000003);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000003);
    etna_set_state(stream, VIVS_TS_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_TS_FLUSH_CACHE, 0x00000001);

    etna_stall(stream, SYNC_RECIPIENT_RA, SYNC_RECIPIENT_PE);
    etna_set_state_reloc(stream, VIVS_TS_COLOR_STATUS_BASE, &at->color_status_base);
    etna_set_state_reloc(stream, VIVS_TS_COLOR_SURFACE_BASE, &at->color_surface_base);
    etna_set_state(stream, VIVS_TS_COLOR_CLEAR_VALUE, 0xff808080);
    etna_set_state(stream, VIVS_RS_UNK016BC, 0xff808080);
    etna_set_state(stream, VIVS_RS_EXTRA_CONFIG, 0x10000000);
    etna_set_state(stream, VIVS_RS_SOURCE_STRIDE, 0x80005000);
    etna_set_state(stream, VIVS_RS_UNK016B0, 0x00010400);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000002);
    etna_set_state(stream, VIVS_DUMMY_DUMMY, 0x00000000);
    etna_set_state(stream, VIVS_TS_MEM_CONFIG, 0x00000041);
}

/* flush cmdbuf from blob */
static void build_flush_cmdbuf(struct etna_cmd_stream *stream, struct attachments *at)
{
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000003);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000003);
    etna_set_state(stream, VIVS_TS_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_DUMMY_DUMMY, 0x00000000);
    etna_set_state(stream, VIVS_TS_MEM_CONFIG, 0x00000000);
}

/* render frame from etnaviv */
static void build_etna_cmdbuf(struct etna_cmd_stream *stream, struct attachments *at)
{
    etna_set_state(stream, VIVS_GL_API_MODE, 0x00000000);
    etna_set_state(stream, VIVS_GL_VERTEX_ELEMENT_CONFIG, 0x00000001);
    etna_set_state(stream, VIVS_RA_EARLY_DEPTH, 0x00000031);
    etna_set_state(stream, VIVS_PA_W_CLIP_LIMIT, 0x34000001);
    etna_set_state(stream, VIVS_PA_FLAGS, 0x00000000);
    etna_set_state(stream, VIVS_RS_SINGLE_BUFFER, 0x00000001);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000003);
    etna_set_state(stream, VIVS_GL_SEMAPHORE_TOKEN, 0x00000705);
    etna_set_state(stream, VIVS_GL_STALL_TOKEN, 0x00000705);
    etna_set_state(stream, VIVS_TS_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_RS_CONFIG, 0x00004606);
    etna_set_state(stream, VIVS_RS_SOURCE_STRIDE, 0x00000000);
    etna_set_state(stream, VIVS_RS_DEST_STRIDE, 0x00000100);
    etna_set_state_reloc(stream, VIVS_RS_PIPE_DEST_ADDR(0), &at->color_status_base);
    etna_set_state(stream, VIVS_RS_PIPE_OFFSET(0), 0x00000000);
    etna_set_state(stream, VIVS_RS_PIPE_OFFSET(1), 0x00000000);
    etna_set_state(stream, VIVS_RS_WINDOW_SIZE, 0x01080010);
    etna_set_state(stream, VIVS_RS_DITHER(0), 0xffffffff);
    etna_set_state(stream, VIVS_RS_DITHER(1), 0xffffffff);
    etna_set_state(stream, VIVS_RS_CLEAR_CONTROL, 0x0001ffff);
    etna_set_state(stream, VIVS_RS_FILL_VALUE(0), 0x55555555);
    etna_set_state(stream, VIVS_RS_FILL_VALUE(1), 0x00000000);
    etna_set_state(stream, VIVS_RS_FILL_VALUE(2), 0x00000000);
    etna_set_state(stream, VIVS_RS_FILL_VALUE(3), 0x00000000);
    etna_set_state(stream, VIVS_RS_EXTRA_CONFIG, 0x00000000);
    etna_set_state(stream, VIVS_RS_KICKER, 0xbeebbeeb);
    etna_set_state(stream, VIVS_GL_SEMAPHORE_TOKEN, 0x00000705);
    etna_set_state(stream, VIVS_GL_STALL_TOKEN, 0x00000705);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000007);
    etna_set_state(stream, VIVS_GL_SEMAPHORE_TOKEN, 0x00000705);
    etna_set_state(stream, VIVS_GL_STALL_TOKEN, 0x00000705);
    etna_set_state(stream, VIVS_FE_VERTEX_ELEMENT_CONFIG(0), 0x10000008);
    etna_set_state(stream, VIVS_FE_VERTEX_ELEMENT_CONFIG(1), 0x20100088);
    etna_set_state(stream, VIVS_GL_MULTI_SAMPLE_CONFIG, 0x000000f0);
    etna_set_state(stream, VIVS_FE_INDEX_STREAM_CONTROL, 0x00000000);
    etna_set_state_reloc(stream, VIVS_FE_VERTEX_STREAM_BASE_ADDR, &at->stream_base);
    etna_set_state(stream, VIVS_FE_VERTEX_STREAM_CONTROL, 0x00000020);
    etna_set_state(stream, VIVS_FE_PRIMITIVE_RESTART_INDEX, 0x00000000);
    etna_set_state(stream, VIVS_VS_END_PC, 0x00000001);
    etna_set_state(stream, VIVS_VS_OUTPUT_COUNT, 0x00000002);
    etna_set_state(stream, VIVS_VS_INPUT_COUNT, 0x00000102);
    etna_set_state(stream, VIVS_VS_TEMP_REGISTER_CONTROL, 0x00000002);
    etna_set_state(stream, VIVS_VS_OUTPUT(0), 0x00000100);
    etna_set_state(stream, VIVS_VS_OUTPUT(1), 0x00000000);
    etna_set_state(stream, VIVS_VS_OUTPUT(2), 0x00000000);
    etna_set_state(stream, VIVS_VS_OUTPUT(3), 0x00000000);
    etna_set_state(stream, VIVS_VS_INPUT(0), 0x00000100);
    etna_set_state(stream, VIVS_VS_INPUT(1), 0x00000000);
    etna_set_state(stream, VIVS_VS_INPUT(2), 0x00000000);
    etna_set_state(stream, VIVS_VS_INPUT(3), 0x00000000);
    etna_set_state(stream, VIVS_VS_LOAD_BALANCING, 0x0f3f0221);
    etna_set_state(stream, VIVS_VS_START_PC, 0x00000000);
    etna_set_state_fixp(stream, VIVS_PA_VIEWPORT_SCALE_X, 0x02800000);
    etna_set_state_fixp(stream, VIVS_PA_VIEWPORT_SCALE_Y, 0xfe700000);
    etna_set_state(stream, VIVS_PA_VIEWPORT_SCALE_Z, 0x3f800000);
    etna_set_state_fixp(stream, VIVS_PA_VIEWPORT_OFFSET_X, 0x02800000);
    etna_set_state_fixp(stream, VIVS_PA_VIEWPORT_OFFSET_Y, 0x01900000);
    etna_set_state(stream, VIVS_PA_VIEWPORT_OFFSET_Z, 0x00000000);
    etna_set_state(stream, VIVS_PA_LINE_WIDTH, 0x3f000000);
    etna_set_state(stream, VIVS_PA_POINT_SIZE, 0x3f000000);
    etna_set_state(stream, VIVS_PA_SYSTEM_MODE, 0x00000011);
    etna_set_state(stream, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT, 0x00000100);
    etna_set_state(stream, VIVS_PA_CONFIG, 0x00412110);
    etna_set_state(stream, VIVS_PA_WIDE_LINE_WIDTH0, 0x3f000000);
    etna_set_state(stream, VIVS_PA_WIDE_LINE_WIDTH1, 0x3f000000);
    etna_set_state(stream, VIVS_PA_SHADER_ATTRIBUTES(0), 0x000002f1);
    etna_set_state(stream, VIVS_PA_SHADER_ATTRIBUTES(1), 0x00000000);
    etna_set_state(stream, VIVS_PA_SHADER_ATTRIBUTES(2), 0x00000000);
    etna_set_state(stream, VIVS_PA_SHADER_ATTRIBUTES(3), 0x00000000);
    etna_set_state(stream, VIVS_PA_SHADER_ATTRIBUTES(4), 0x00000000);
    etna_set_state(stream, VIVS_PA_SHADER_ATTRIBUTES(5), 0x00000000);
    etna_set_state(stream, VIVS_PA_SHADER_ATTRIBUTES(6), 0x00000000);
    etna_set_state(stream, VIVS_PA_SHADER_ATTRIBUTES(7), 0x00000000);
    etna_set_state(stream, VIVS_PA_SHADER_ATTRIBUTES(8), 0x00000000);
    etna_set_state(stream, VIVS_PA_SHADER_ATTRIBUTES(9), 0x00000000);
    etna_set_state_fixp(stream, VIVS_SE_SCISSOR_LEFT, 0x00000000);
    etna_set_state_fixp(stream, VIVS_SE_SCISSOR_TOP, 0x00000000);
    etna_set_state_fixp(stream, VIVS_SE_SCISSOR_RIGHT, 0x05001119);
    etna_set_state_fixp(stream, VIVS_SE_SCISSOR_BOTTOM, 0x03201111);
    etna_set_state(stream, VIVS_SE_DEPTH_SCALE, 0x00000000);
    etna_set_state(stream, VIVS_SE_DEPTH_BIAS, 0x00000000);
    etna_set_state(stream, VIVS_SE_CONFIG, 0x00000000);
    etna_set_state_fixp(stream, VIVS_SE_CLIP_RIGHT, 0x0500ffff);
    etna_set_state_fixp(stream, VIVS_SE_CLIP_BOTTOM, 0x0320ffff);
    etna_set_state(stream, VIVS_RA_CONTROL, 0x00000001);
    etna_set_state(stream, VIVS_RA_MULTISAMPLE_UNK00E04, 0x00000000);
    etna_set_state(stream, VIVS_RA_MULTISAMPLE_UNK00E10(0), 0x00000000);
    etna_set_state(stream, VIVS_RA_MULTISAMPLE_UNK00E10(1), 0x00000000);
    etna_set_state(stream, VIVS_RA_MULTISAMPLE_UNK00E10(2), 0x00000000);
    etna_set_state(stream, VIVS_RA_MULTISAMPLE_UNK00E10(3), 0x00000000);
    etna_set_state(stream, VIVS_RA_CENTROID_TABLE(0), 0x00000000);
    etna_set_state(stream, VIVS_RA_CENTROID_TABLE(1), 0x00000000);
    etna_set_state(stream, VIVS_RA_CENTROID_TABLE(2), 0x00000000);
    etna_set_state(stream, VIVS_RA_CENTROID_TABLE(3), 0x00000000);
    etna_set_state(stream, VIVS_RA_CENTROID_TABLE(4), 0x00000000);
    etna_set_state(stream, VIVS_RA_CENTROID_TABLE(5), 0x00000000);
    etna_set_state(stream, VIVS_RA_CENTROID_TABLE(6), 0x00000000);
    etna_set_state(stream, VIVS_RA_CENTROID_TABLE(7), 0x00000000);
    etna_set_state(stream, VIVS_RA_CENTROID_TABLE(8), 0x00000000);
    etna_set_state(stream, VIVS_RA_CENTROID_TABLE(9), 0x00000000);
    etna_set_state(stream, VIVS_RA_CENTROID_TABLE(10), 0x00000000);
    etna_set_state(stream, VIVS_RA_CENTROID_TABLE(11), 0x00000000);
    etna_set_state(stream, VIVS_RA_CENTROID_TABLE(12), 0x00000000);
    etna_set_state(stream, VIVS_RA_CENTROID_TABLE(13), 0x00000000);
    etna_set_state(stream, VIVS_RA_CENTROID_TABLE(14), 0x00000000);
    etna_set_state(stream, VIVS_RA_CENTROID_TABLE(15), 0x00000000);
    etna_set_state(stream, VIVS_PS_END_PC, 0x00000001);
    etna_set_state(stream, VIVS_PS_OUTPUT_REG, 0x00000001);
    etna_set_state(stream, VIVS_PS_INPUT_COUNT, 0x00001f02);
    etna_set_state(stream, VIVS_PS_TEMP_REGISTER_CONTROL, 0x00000002);
    etna_set_state(stream, VIVS_PS_CONTROL, 0x00000002);
    etna_set_state(stream, VIVS_PS_START_PC, 0x00000000);
    etna_set_state(stream, VIVS_PE_DEPTH_CONFIG, 0x05000711);
    etna_set_state(stream, VIVS_PE_DEPTH_NEAR, 0x00000000);
    etna_set_state(stream, VIVS_PE_DEPTH_FAR, 0x3f800000);
    etna_set_state(stream, VIVS_PE_DEPTH_NORMALIZE, 0x4b7fffff);
    etna_set_state(stream, VIVS_PE_DEPTH_STRIDE, 0x00001400);
    etna_set_state(stream, VIVS_PE_STENCIL_OP, 0x00000000);
    etna_set_state(stream, VIVS_PE_STENCIL_CONFIG, 0x00000000);
    etna_set_state(stream, VIVS_PE_ALPHA_OP, 0x00000000);
    etna_set_state(stream, VIVS_PE_ALPHA_BLEND_COLOR, 0x00000000);
    etna_set_state(stream, VIVS_PE_ALPHA_CONFIG, 0x00000000);
    etna_set_state(stream, VIVS_PE_COLOR_FORMAT, 0x00110f05);
    etna_set_state(stream, VIVS_PE_COLOR_STRIDE, 0x00001400);
    etna_set_state(stream, VIVS_PE_HDEPTH_CONTROL, 0x00000000);
    etna_set_state_reloc(stream, VIVS_PE_PIPE_COLOR_ADDR(0), &at->color_surface_base);
    etna_set_state_reloc(stream, VIVS_PE_PIPE_COLOR_ADDR(1), &at->color_surface_base);
    etna_set_state_reloc(stream, VIVS_PE_PIPE_DEPTH_ADDR(0), &at->depth_surface_base);
    etna_set_state_reloc(stream, VIVS_PE_PIPE_DEPTH_ADDR(1), &at->depth_surface_base);
    etna_set_state(stream, VIVS_PE_STENCIL_CONFIG_EXT, 0x00000000);
    etna_set_state(stream, VIVS_PE_LOGIC_OP, 0x000e420c);
    etna_set_state(stream, VIVS_PE_DITHER(0), 0x6e4ca280);
    etna_set_state(stream, VIVS_PE_DITHER(1), 0x5d7f91b3);
    etna_set_state(stream, VIVS_TS_MEM_CONFIG, 0x00000003);
    etna_set_state_reloc(stream, VIVS_TS_COLOR_STATUS_BASE, &at->color_status_base);
    etna_set_state_reloc(stream, VIVS_TS_COLOR_SURFACE_BASE, &at->color_surface_base);
    etna_set_state(stream, VIVS_TS_COLOR_CLEAR_VALUE, 0xff808080);
    etna_set_state_reloc(stream, VIVS_TS_DEPTH_STATUS_BASE, &at->depth_status_base);
    etna_set_state_reloc(stream, VIVS_TS_DEPTH_SURFACE_BASE, &at->depth_surface_base);
    etna_set_state(stream, VIVS_TS_DEPTH_CLEAR_VALUE, 0x00000000);
    etna_set_state(stream, VIVS_TE_SAMPLER_CONFIG0(0), 0x00000000);
    etna_set_state(stream, VIVS_TE_SAMPLER_CONFIG0(1), 0x00000000);
    etna_set_state(stream, VIVS_TE_SAMPLER_CONFIG0(2), 0x00000000);
    etna_set_state(stream, VIVS_TE_SAMPLER_CONFIG0(3), 0x00000000);
    etna_set_state(stream, VIVS_TE_SAMPLER_CONFIG0(4), 0x00000000);
    etna_set_state(stream, VIVS_TE_SAMPLER_CONFIG0(5), 0x00000000);
    etna_set_state(stream, VIVS_TE_SAMPLER_CONFIG0(6), 0x00000000);
    etna_set_state(stream, VIVS_TE_SAMPLER_CONFIG0(7), 0x00000000);
    etna_set_state(stream, VIVS_TE_SAMPLER_CONFIG0(8), 0x00000000);
    etna_set_state(stream, VIVS_TE_SAMPLER_CONFIG0(9), 0x00000000);
    etna_set_state(stream, VIVS_TE_SAMPLER_CONFIG0(10), 0x00000000);
    etna_set_state(stream, VIVS_TE_SAMPLER_CONFIG0(11), 0x00000000);
    etna_set_state(stream, VIVS_GL_VARYING_TOTAL_COMPONENTS, 0x00000004);
    etna_set_state(stream, VIVS_GL_VARYING_NUM_COMPONENTS, 0x00000004);
    etna_set_state(stream, VIVS_GL_VARYING_COMPONENT_USE(0), 0x00000000);
    etna_set_state(stream, VIVS_GL_VARYING_COMPONENT_USE(1), 0x00000000);
    etna_set_state(stream, VIVS_GL_SEMAPHORE_TOKEN, 0x00000701);
    etna_cmd_stream_emit(stream, 0x48000000); /* command STALL (9) OP=STALL */
    etna_cmd_stream_emit(stream, 0x00000701); /* command   TOKEN FROM=FE,TO=PE */
    etna_set_state(stream, VIVS_VS_ICACHE_CONTROL, 0x00000030);
    etna_set_state(stream, VIVS_VS_RANGE, 0x00000000);
    etna_set_state(stream, VIVS_PS_RANGE, 0x01000100);
    etna_set_state(stream, VIVS_SH_INST_MEM(0), 0x00000000);
    etna_set_state(stream, VIVS_SH_INST_MEM(1), 0x00000000);
    etna_set_state(stream, VIVS_SH_INST_MEM(2), 0x00000000);
    etna_set_state(stream, VIVS_SH_INST_MEM(3), 0x00000000);
    etna_set_state(stream, VIVS_SH_INST_MEM_MIRROR(1024), 0x00000000);
    etna_set_state(stream, VIVS_SH_INST_MEM_MIRROR(1025), 0x00000000);
    etna_set_state(stream, VIVS_SH_INST_MEM_MIRROR(1026), 0x00000000);
    etna_set_state(stream, VIVS_SH_INST_MEM_MIRROR(1027), 0x00000000);
    etna_set_state(stream, VIVS_VS_UNIFORM_BASE, 0x00000000);
    etna_set_state(stream, VIVS_PS_UNIFORM_BASE, 0x000000a8);
    etna_set_state(stream, VIVS_VS_UNIFORM_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_VS_UNIFORM_CACHE, 0x00000011);
    etna_cmd_stream_emit(stream, 0x28000000); /* command DRAW_PRIMITIVES (5) OP=DRAW_PRIMITIVES */
    etna_cmd_stream_emit(stream, 0x00000005); /* command   COMMAND TYPE=TRIANGLE_STRIP */
    etna_cmd_stream_emit(stream, 0x00000000); /* command   START 0x0 */
    etna_cmd_stream_emit(stream, 0x00000001); /* command   COUNT 0x1 */
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000003);
    etna_set_state(stream, VIVS_GL_SEMAPHORE_TOKEN, 0x00000705);
    etna_set_state(stream, VIVS_GL_STALL_TOKEN, 0x00000705);

    etna_set_state(stream, VIVS_TS_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_TS_MEM_CONFIG, 0x00000002);
    etna_set_state_reloc(stream, VIVS_TS_COLOR_STATUS_BASE, &at->color_status_base);
    etna_set_state_reloc(stream, VIVS_TS_COLOR_SURFACE_BASE, &at->color_surface_base);
    etna_set_state(stream, VIVS_TS_COLOR_CLEAR_VALUE, 0xff808080);
    etna_set_state(stream, VIVS_RS_CONFIG, 0x00000686);
    etna_set_state(stream, VIVS_RS_SOURCE_STRIDE, 0x80005000);
    etna_set_state(stream, VIVS_RS_DEST_STRIDE, 0x00001400);
    etna_set_state_reloc(stream, VIVS_RS_PIPE_SOURCE_ADDR(0), &at->color_surface_base);
    etna_set_state_reloc(stream, VIVS_RS_PIPE_DEST_ADDR(0), &at->color_surface_base);
    etna_set_state(stream, VIVS_RS_PIPE_OFFSET(0), 0x00000000);
    etna_set_state(stream, VIVS_RS_PIPE_OFFSET(1), 0x00000000);
    etna_set_state(stream, VIVS_RS_WINDOW_SIZE, 0x03200500);
    etna_set_state(stream, VIVS_RS_DITHER(0), 0xffffffff);
    etna_set_state(stream, VIVS_RS_DITHER(1), 0xffffffff);
    etna_set_state(stream, VIVS_RS_CLEAR_CONTROL, 0x00000000);
    etna_set_state(stream, VIVS_RS_FILL_VALUE(0), 0x00000000);
    etna_set_state(stream, VIVS_RS_FILL_VALUE(1), 0x00000000);
    etna_set_state(stream, VIVS_RS_FILL_VALUE(2), 0x00000000);
    etna_set_state(stream, VIVS_RS_FILL_VALUE(3), 0x00000000);
    etna_set_state(stream, VIVS_RS_EXTRA_CONFIG, 0x00000000);
    etna_set_state(stream, VIVS_RS_KICKER, 0xbeebbeeb);
}

static void process_output(struct attachments *at)
{
    void *mapped = etna_bo_map(at->color_surface);
    printf("* Writing output\n");

    /** Write to disk or something */
    int fd = open("triangle_out.dat", O_RDWR|O_CREAT|O_TRUNC, 0644);
    if(fd >= 0) {
        write(fd, mapped, color_surface_size);
        close(fd);
    } else {
        fprintf(stderr, "Unable to write output\n");
    }
}

int main(int argc, char *argv[])
{
    struct drm_test_info *info;
    if ((info = drm_test_setup(argc, argv)) == NULL) {
        return 1;
    }
    enum hardware_type hwt = drm_cl_get_hardware_type(info);
    if (hwt != HWT_GC3000) {
        fprintf(stderr, "This test only runs on GC3000\n");
        goto out;
    }
    struct attachments at = {};
    if (!initialize_attachments(info->dev, &at)) {
        fprintf(stderr, "Failed to initialize attachments\n");
        goto out;
    }
#if 1 /* use command buffer from blob */
    build_frame_cmdbuf(info->stream, &at);
    printf("* Rendering\n");
    etna_cmd_stream_finish(info->stream);

    build_flush_cmdbuf(info->stream, &at);
    printf("* Flushing\n");
    etna_cmd_stream_finish(info->stream);
#else
    build_etna_cmdbuf(info->stream, &at);
    printf("* Etna rendering\n");
    etna_cmd_stream_finish(info->stream);
#endif

    process_output(&at);

    free_attachments(info->dev, &at);
    drm_test_teardown(info);
    return 0;
out:
    drm_test_teardown(info);
    return 1;
}
