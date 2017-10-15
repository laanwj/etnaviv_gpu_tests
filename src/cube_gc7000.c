#define _POSIX_C_SOURCE 200112L
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "write_bmp.h"
#include "memutil.h"

#include "drm_setup.h"
#include "cmdstream.h"
#include "etna_fb.h"

#include <state.xml.h>
#include <state_3d.xml.h>
#include <state_blt.xml.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct test_info {
    struct etna_bo *bo_color;
    struct etna_bo *bo_depth;
    struct etna_bo *bo_color_ts;
    struct etna_bo *bo_depth_ts;
    struct etna_bo *bo_vertex;
    struct etna_bo *bo_txdesc;
    struct etna_bo *bo_vs;
    struct etna_bo *bo_ps;
    struct etna_bo *bo_bmp;

    struct etna_reloc ADDR_RENDER_TARGET_A;
    struct etna_reloc ADDR_DEPTH_A;
    struct etna_reloc ADDR_TILE_STATUS_A; /* Depth TS */
    struct etna_reloc ADDR_TILE_STATUS_B; /* Color TS */
    struct etna_reloc ADDR_VERTEX_A; /* Vertex buffer */
    struct etna_reloc ADDR_TXDESC_A; /* Dummy TX descriptor */
    struct etna_reloc ADDR_ICACHE_A; /* Vertex shader */
    struct etna_reloc ADDR_ICACHE_B; /* Pixel shader */
    struct etna_reloc ADDR_USER_A; /* Bitmap out */
    struct fb_info *fb;
};

float vertex_data[] = {
    // front
    -1.0f, -1.0f, +1.0f, // point blue
    0.0f,  0.0f,  1.0f, // blue
    +0.0f, +0.0f, +1.0f, // forward

    +1.0f, -1.0f, +1.0f, // point magenta
    1.0f,  0.0f,  1.0f, // magenta
    +0.0f, +0.0f, +1.0f, // forward

    -1.0f, +1.0f, +1.0f, // point cyan
    0.0f,  1.0f,  1.0f, // cyan
    +0.0f, +0.0f, +1.0f, // forward

    +1.0f, +1.0f, +1.0f, // point white
    1.0f,  1.0f,  1.0f, // white
    +0.0f, +0.0f, +1.0f, // forward

    // back
    +1.0f, -1.0f, -1.0f, // point red
    1.0f,  0.0f,  0.0f, // red
    +0.0f, +0.0f, -1.0f, // backbard

    -1.0f, -1.0f, -1.0f, // point black
    0.0f,  0.0f,  0.0f, // black
    +0.0f, +0.0f, -1.0f, // backbard

    +1.0f, +1.0f, -1.0f, // point yellow
    1.0f,  1.0f,  0.0f, // yellow
    +0.0f, +0.0f, -1.0f, // backbard

    -1.0f, +1.0f, -1.0f, // point green
    0.0f,  1.0f,  0.0f, // green
    +0.0f, +0.0f, -1.0f, // backbard

    // right
    +1.0f, -1.0f, +1.0f, // point magenta
    1.0f,  0.0f,  1.0f, // magenta
    +1.0f, +0.0f, +0.0f, // right

    +1.0f, -1.0f, -1.0f, // point red
    1.0f,  0.0f,  0.0f, // red
    +1.0f, +0.0f, +0.0f, // right

    +1.0f, +1.0f, +1.0f, // point white
    1.0f,  1.0f,  1.0f, // white
    +1.0f, +0.0f, +0.0f, // right

    +1.0f, +1.0f, -1.0f, // point yellow
    1.0f,  1.0f,  0.0f, // yellow
    +1.0f, +0.0f, +0.0f, // right

    // left
    -1.0f, -1.0f, -1.0f, // point black
    0.0f,  0.0f,  0.0f, // black
    -1.0f, +0.0f, +0.0f, // left

    -1.0f, -1.0f, +1.0f, // point blue
    0.0f,  0.0f,  1.0f, // blue
    -1.0f, +0.0f, +0.0f, // left

    -1.0f, +1.0f, -1.0f, // point green
    0.0f,  1.0f,  0.0f, // green
    -1.0f, +0.0f, +0.0f, // left

    -1.0f, +1.0f, +1.0f, // point cyan
    0.0f,  1.0f,  1.0f, // cyan
    -1.0f, +0.0f, +0.0f, // left

    // top
    -1.0f, +1.0f, +1.0f, // point cyan
    0.0f,  1.0f,  1.0f, // cyan
    +0.0f, +1.0f, +0.0f, // up

    +1.0f, +1.0f, +1.0f, // point white
    1.0f,  1.0f,  1.0f, // white
    +0.0f, +1.0f, +0.0f, // up

    -1.0f, +1.0f, -1.0f, // point green
    0.0f,  1.0f,  0.0f, // green
    +0.0f, +1.0f, +0.0f, // up

    +1.0f, +1.0f, -1.0f, // point yellow
    1.0f,  1.0f,  0.0f, // yellow
    +0.0f, +1.0f, +0.0f, // up

    // bottom
    -1.0f, -1.0f, -1.0f, // point black
    0.0f,  0.0f,  0.0f, // black
    +0.0f, -1.0f, +0.0f, // down

    +1.0f, -1.0f, -1.0f, // point red
    1.0f,  0.0f,  0.0f, // red
    +0.0f, -1.0f, +0.0f, // down

    -1.0f, -1.0f, +1.0f, // point blue
    0.0f,  0.0f,  1.0f, // blue
    +0.0f, -1.0f, +0.0f, // down

    +1.0f, -1.0f, +1.0f,  // point magenta
    1.0f,  0.0f,  1.0f,  // magenta
    +0.0f, -1.0f, +0.0f  // down
};
/* Vertex shader */
uint32_t vs_data[] = {
/*   0: */ 0x07831003, 0x39005804, 0x00aa0050, 0x00000000,  /* mul	t3, u5, t0.yyyy, void */
/*   1: */ 0x07831002, 0x39004804, 0x00000050, 0x00390038,  /* mad	t3, u4, t0.xxxx, t3 */
/*   2: */ 0x07831002, 0x39006804, 0x01540050, 0x00390038,  /* mad	t3, u6, t0.zzzz, t3 */
/*   3: */ 0x07831002, 0x39007804, 0x01fe0050, 0x00390038,  /* mad	t3, u7, t0.wwww, t3 */
/*   4: */ 0x03841003, 0x29009804, 0x00aa00d0, 0x00000000,  /* mul	t4.xyz_, u9.xyzz, t1.yyyy, void */
/*   5: */ 0x03841002, 0x29008804, 0x000000d0, 0x00290048,  /* mad	t4.xyz_, u8.xyzz, t1.xxxx, t4.xyzz */
/*   6: */ 0x03811002, 0x2900a804, 0x015400d0, 0x00290048,  /* mad	t1.xyz_, u10.xyzz, t1.zzzz, t4.xyzz */
/*   7: */ 0x07841003, 0x39001804, 0x00aa0050, 0x00000000,  /* mul	t4, u1, t0.yyyy, void */
/*   8: */ 0x07841002, 0x39000804, 0x00000050, 0x00390048,  /* mad	t4, u0, t0.xxxx, t4 */
/*   9: */ 0x07841002, 0x39002804, 0x01540050, 0x00390048,  /* mad	t4, u2, t0.zzzz, t4 */
/*  10: */ 0x07801002, 0x39003804, 0x01fe0050, 0x00390048,  /* mad	t0, u3, t0.wwww, t4 */
/*  11: */ 0x0401100c, 0x00000004, 0x00000000, 0x003fc008,  /* rcp	t1.___w, void, void, t0.wwww */
/*  12: */ 0x03801002, 0x69000804, 0x01fe00c0, 0x202900b8,  /* mad	t0.xyz_, -t0.xyzz, t1.wwww, u11.xyzz */
/*  13: */ 0x04001035, 0x29000804, 0x00010000, 0x00000000,  /* norm_dp3	t0.___w, t0.xyzz, void, void */
/*  14: */ 0x0400100d, 0x00000004, 0x00000000, 0x003fc008,  /* rsq	t0.___w, void, void, t0.wwww */
/*  15: */ 0x0b801037, 0x29000804, 0x01ff0040, 0x00000000,  /* norm_mul	t0.xyz_, t0.xyzz, t0.wwww, void */
/*  16: */ 0x00801005, 0x29001804, 0x01480040, 0x00000000,  /* dp3	t0.x___, t1.xyzz, t0.xyzz, void */
/*  17: */ 0x0080108f, 0x00000804, 0x00000078, 0x70000008,  /* select.lt	t0.x___, 0.0, t0.xxxx, 0.0 */
/*  18: */ 0x03801003, 0x00000804, 0x01480140, 0x00000000,  /* mul	t0.xyz_, t0.xxxx, t2.xyzz, void */
/*  19: */ 0x04001009, 0x00000004, 0x00000000, 0x707f0008,  /* mov	t0.___w, void, void, 1.0 */
};
/* Fragment shader */
uint32_t ps_data[] = {
/*   0: */ 0x00000000, 0x00000000, 0x00000000, 0x00000000,  /* nop	void, void, void, void */
};
/* Texture descriptor - all zeros */
uint32_t txdesc_data[256/4];

void init_reloc(struct etna_reloc *reloc, struct etna_bo *bo, uint32_t offset, uint32_t flags)
{
    reloc->bo = bo;
    reloc->offset = offset;
    reloc->flags = flags;
}

struct test_info *test_init(struct etna_device *conn, int fbdev)
{
    struct test_info *info = CALLOC_STRUCT(test_info);
    assert(sizeof(vertex_data) == 0x360);
    assert(sizeof(vs_data) == 0x140);
    assert(sizeof(ps_data) == 0x10);
    assert(sizeof(txdesc_data) == 0x100);
    /* Allocate bos */
    info->bo_color = etna_bo_new(conn, 0x7f8000, DRM_ETNA_GEM_TYPE_RT);
    assert(info->bo_color);
    info->bo_depth = etna_bo_new(conn, 0x7f8000, DRM_ETNA_GEM_TYPE_ZS);
    assert(info->bo_depth);
    info->bo_color_ts = etna_bo_new(conn, 0x3fc0, DRM_ETNA_GEM_TYPE_TS);
    assert(info->bo_color_ts);
    info->bo_depth_ts = etna_bo_new(conn, 0x3fc0, DRM_ETNA_GEM_TYPE_TS);
    assert(info->bo_depth_ts);
    info->bo_txdesc = etna_bo_new(conn, sizeof(txdesc_data), DRM_ETNA_GEM_TYPE_TXD);
    assert(info->bo_txdesc);
    info->bo_vertex = etna_bo_new(conn, sizeof(vertex_data), DRM_ETNA_GEM_TYPE_VTX);
    assert(info->bo_vertex);
    info->bo_vs = etna_bo_new(conn, sizeof(vs_data), DRM_ETNA_GEM_TYPE_IC);
    assert(info->bo_vs);
    info->bo_ps = etna_bo_new(conn, sizeof(ps_data), DRM_ETNA_GEM_TYPE_IC);
    assert(info->bo_ps);
    if (!fbdev) {
        info->bo_bmp = etna_bo_new(conn, 0x7e9000, DRM_ETNA_GEM_TYPE_BMP);
        assert(info->bo_bmp);
    }

    /* Initialize relocs */
    init_reloc(&info->ADDR_RENDER_TARGET_A, info->bo_color, 0x0, ETNA_RELOC_READ|ETNA_RELOC_WRITE);
    init_reloc(&info->ADDR_DEPTH_A, info->bo_depth, 0x0, ETNA_RELOC_READ|ETNA_RELOC_WRITE);
    init_reloc(&info->ADDR_TILE_STATUS_A, info->bo_depth_ts, 0x0, ETNA_RELOC_READ|ETNA_RELOC_WRITE);
    init_reloc(&info->ADDR_TILE_STATUS_B, info->bo_color_ts, 0x0, ETNA_RELOC_READ|ETNA_RELOC_WRITE);
    init_reloc(&info->ADDR_VERTEX_A, info->bo_vertex, 0x0, ETNA_RELOC_READ|ETNA_RELOC_WRITE);
    init_reloc(&info->ADDR_TXDESC_A, info->bo_txdesc, 0x0, ETNA_RELOC_READ|ETNA_RELOC_WRITE);
    init_reloc(&info->ADDR_ICACHE_A, info->bo_vs, 0x0, ETNA_RELOC_READ|ETNA_RELOC_WRITE);
    init_reloc(&info->ADDR_ICACHE_B, info->bo_ps, 0x0, ETNA_RELOC_READ|ETNA_RELOC_WRITE);
    if (!fbdev) {
        init_reloc(&info->ADDR_USER_A, info->bo_bmp, 0x0, ETNA_RELOC_READ|ETNA_RELOC_WRITE);
    }

    /* Initial content */
    memcpy(etna_bo_map(info->bo_vertex), vertex_data, sizeof(vertex_data));
    memcpy(etna_bo_map(info->bo_vs), vs_data, sizeof(vs_data));
    memcpy(etna_bo_map(info->bo_ps), ps_data, sizeof(ps_data));
    memcpy(etna_bo_map(info->bo_txdesc), txdesc_data, sizeof(txdesc_data));

    if (!fbdev) {
        memset(etna_bo_map(info->bo_bmp), 0, 0x7e9000);
    } else {
        int rv = fb_open(conn, 0, &info->fb);
        assert(rv == 0);
        /* HD or bust... */
        assert(info->fb->width == 1920 && info->fb->height == 1080 && info->fb->rs_format==RS_FORMAT_A8R8G8B8);
        fb_set_buffer(info->fb, 0);
        info->ADDR_USER_A = info->fb->buffer[0];
    }
    return info;
}

void test_free(struct etna_device *conn, struct test_info *info)
{
    etna_bo_del(info->bo_color);
    etna_bo_del(info->bo_depth);
    etna_bo_del(info->bo_color_ts);
    etna_bo_del(info->bo_depth_ts);
    etna_bo_del(info->bo_txdesc);
    etna_bo_del(info->bo_vertex);
    etna_bo_del(info->bo_vs);
    etna_bo_del(info->bo_ps);
    etna_bo_del(info->bo_bmp);
    free(info);
}

void gen_cmdbuf_1(struct etna_cmd_stream *stream, struct test_info *info)
{
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000c23);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000c23);
    etna_set_state(stream, VIVS_TS_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_TS_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_FE_HALTI5_UNK007D8, 0x00000002);
    etna_set_state(stream, VIVS_RA_CONTROL, 0x00000001);
    etna_set_state(stream, VIVS_PA_W_CLIP_LIMIT, 0x34000001);
    etna_set_state(stream, VIVS_PE_LOGIC_OP, 0xfffffbff);
    etna_set_state(stream, VIVS_RA_UNK00E0C, 0x00000000);
    etna_set_state(stream, VIVS_PA_FLAGS, 0x40000000);
    etna_set_state(stream, VIVS_VS_HALTI5_UNK15600, 0x00000002);
    etna_set_state(stream, VIVS_VS_HALTI1_UNK00884, 0x00000808);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_UNK14C40, 0x00000001);
    etna_set_state(stream, VIVS_PA_SYSTEM_MODE, 0x00000011);
    etna_set_state(stream, VIVS_GL_API_MODE, 0x00000000);
    etna_set_state(stream, VIVS_SE_CONFIG, 0x00000000);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000c23);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000c23);
    etna_set_state(stream, VIVS_TS_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_TS_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000c23);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000c23);
    etna_set_state(stream, VIVS_TS_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000c23);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000c23);
    etna_set_state(stream, VIVS_TS_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000001);
    etna_set_state(stream, VIVS_BLT_CONFIG, 0x00000180);
    etna_set_state(stream, VIVS_BLT_DEST_STRIDE, 0x60201e00);
    etna_set_state(stream, VIVS_BLT_DEST_CONFIG, 0x04020033);
    etna_set_state_reloc(stream, VIVS_BLT_DEST_ADDR, &info->ADDR_RENDER_TARGET_A);
    etna_set_state(stream, VIVS_BLT_SRC_STRIDE, 0x60201e00);
    etna_set_state(stream, VIVS_BLT_SRC_CONFIG, 0x00220033);
    etna_set_state_reloc(stream, VIVS_BLT_SRC_ADDR, &info->ADDR_RENDER_TARGET_A);
    etna_set_state(stream, VIVS_BLT_DEST_POS, 0x00000000);
    etna_set_state(stream, VIVS_BLT_IMAGE_SIZE, 0x04380780);
    etna_set_state(stream, VIVS_BLT_CLEAR_COLOR0, 0xff808080);
    etna_set_state(stream, VIVS_BLT_CLEAR_COLOR1, 0xff808080);
    etna_set_state(stream, VIVS_BLT_UNK1404C, 0xffffffff);
    etna_set_state(stream, VIVS_BLT_UNK14050, 0xffffffff);
    etna_set_state_reloc(stream, VIVS_BLT_DEST_TS, &info->ADDR_TILE_STATUS_B);
    etna_set_state_reloc(stream, VIVS_BLT_SRC_TS, &info->ADDR_TILE_STATUS_B);
    etna_set_state(stream, VIVS_BLT_DEST_TS_CLEAR_VALUE0, 0xff808080);
    etna_set_state(stream, VIVS_BLT_DEST_TS_CLEAR_VALUE1, 0xff808080);
    etna_set_state(stream, VIVS_BLT_SRC_TS_CLEAR_VALUE0, 0xff808080);
    etna_set_state(stream, VIVS_BLT_SRC_TS_CLEAR_VALUE1, 0xff808080);
    etna_set_state(stream, VIVS_BLT_SET_COMMAND, 0x00000003);
    etna_set_state(stream, VIVS_BLT_COMMAND, 0x00000001);
    etna_set_state(stream, VIVS_BLT_SET_COMMAND, 0x00000003);
    etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000000);
    etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000001);
    etna_set_state(stream, VIVS_GL_SEMAPHORE_TOKEN, 0x00001005);
    etna_set_state(stream, VIVS_GL_STALL_TOKEN, 0x00001005);
    etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000000);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000002);
    etna_set_state(stream, VIVS_DUMMY_DUMMY, 0x00000000);
    etna_set_state_reloc(stream, VIVS_TS_COLOR_STATUS_BASE, &info->ADDR_TILE_STATUS_B);
    etna_set_state_reloc(stream, VIVS_TS_COLOR_SURFACE_BASE, &info->ADDR_RENDER_TARGET_A);
    etna_set_state(stream, VIVS_TS_COLOR_CLEAR_VALUE, 0xff808080);
    etna_set_state(stream, VIVS_TS_COLOR_CLEAR_VALUE_EXT, 0xff808080);
    etna_set_state(stream, VIVS_TS_MEM_CONFIG, 0x00000382);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_DUMMY_DUMMY, 0x00000000);
    etna_set_state(stream, VIVS_TS_DEPTH_AUTO_DISABLE_COUNT, 0x00007f80);
    etna_set_state_reloc(stream, VIVS_TS_DEPTH_STATUS_BASE, &info->ADDR_TILE_STATUS_A);
    etna_set_state_reloc(stream, VIVS_TS_DEPTH_SURFACE_BASE, &info->ADDR_DEPTH_A);
    etna_set_state(stream, VIVS_TS_DEPTH_CLEAR_VALUE, 0xffffff00);
    etna_set_state(stream, VIVS_TS_MEM_CONFIG, 0x00004393);
    etna_set_state(stream, VIVS_PS_HALTI4_UNK01054, 0xffff6fff);
    etna_set_state(stream, VIVS_PS_HALTI4_UNK01054, 0x6fffffff);
    etna_set_state(stream, VIVS_PS_HALTI4_UNK01054, 0xf70fffff);
    etna_set_state(stream, VIVS_PS_HALTI4_UNK01054, 0xfff6ffff);
    etna_set_state(stream, VIVS_PS_HALTI4_UNK01054, 0xfffff6ff);
    etna_set_state(stream, VIVS_PS_HALTI4_UNK01054, 0xffffff7f);
    etna_set_state(stream, VIVS_SE_DEPTH_SCALE, 0x00000000);
    etna_set_state(stream, VIVS_SE_DEPTH_BIAS, 0x00000000);
    etna_set_state(stream, VIVS_GL_SEMAPHORE_TOKEN, 0x00000701);
    etna_cmd_stream_emit(stream, 0x48000000); /* command STALL (9) OP=STALL */
    etna_cmd_stream_emit(stream, 0x00000701); /* command   TOKEN FROM=FE,TO=PE,UNK28=0x0 */
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(0), 0x3f3244ed);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(1), 0x3ebd3e53);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(2), 0x3f1d7d34);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(3), 0x00000000);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(4), 0x3dfb782d);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(5), 0x3f487f08);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(6), 0xbf1c0ad3);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(7), 0x00000000);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(8), 0xbf3504f4);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(9), 0x3f000000);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(10), 0x3effffff);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(11), 0x00000000);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(12), 0x00000000);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(13), 0x00000000);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(14), 0xc1000000);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(15), 0x3f800000);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(16), 0x3fbf00b4);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(17), 0x3fb43b5c);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(18), 0xc01d7d34);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(19), 0xbf1d7d34);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(20), 0x3e86b73c);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(21), 0x403ef2e4);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(22), 0x401c0ad3);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(23), 0x3f1c0ad3);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(24), 0xbfc1f305);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(25), 0x3ff3cf3e);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(26), 0xbfffffff);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(27), 0xbeffffff);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(28), 0x00000000);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(29), 0x00000000);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(30), 0x40000000);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(31), 0x41000000);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(32), 0x3f3244ed);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(33), 0x3ebd3e53);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(34), 0x3f1d7d34);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(36), 0x3dfb782d);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(37), 0x3f487f08);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(38), 0xbf1c0ad3);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(40), 0xbf3504f4);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(41), 0x3f000000);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(42), 0x3effffff);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(0), 0x00003008);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(1), 0x000c3008);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(2), 0x00183008);
    etna_set_state(stream, VIVS_VS_HALTI5_UNK008C0(0), 0x00010200);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(0), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(1), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(2), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(0), 0x0000000c);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(1), 0x00000018);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(2), 0x00000824);
    etna_set_state_reloc(stream, VIVS_NFE_VERTEX_STREAMS_BASE_ADDR(0), &info->ADDR_VERTEX_A);
    etna_set_state(stream, VIVS_NFE_VERTEX_STREAMS_CONTROL(0), 0x00000024);
    etna_set_state(stream, VIVS_NFE_VERTEX_STREAMS_UNK14680(0), 0x00000000);
    etna_set_state(stream, VIVS_FE_HALTI5_UNK007C4, 0x00000000);
    etna_set_state_reloc(stream, VIVS_FE_INDEX_STREAM_BASE_ADDR, NULL);
    etna_set_state(stream, VIVS_FE_INDEX_STREAM_CONTROL, 0x00000000);
    etna_set_state(stream, VIVS_FE_PRIMITIVE_RESTART_INDEX, 0x00000000);
    etna_set_state_fixp(stream, VIVS_PA_VIEWPORT_SCALE_X, 0x03c00000);
    etna_set_state_fixp(stream, VIVS_PA_VIEWPORT_SCALE_Y, 0xfde40000);
    etna_set_state_fixp(stream, VIVS_PA_VIEWPORT_OFFSET_X, 0x03c00000);
    etna_set_state_fixp(stream, VIVS_PA_VIEWPORT_OFFSET_Y, 0x021c0000);
    etna_set_state(stream, VIVS_PA_VIEWPORT_UNK00A80, 0x38f01e06);
    etna_set_state_fixp(stream, VIVS_PA_VIEWPORT_UNK00A84, 0x20000000);
    etna_set_state(stream, VIVS_PA_ZFARCLIPPING, 0x00000000);
    etna_set_state_fixp(stream, VIVS_SE_SCISSOR_LEFT, 0x00000000);
    etna_set_state_fixp(stream, VIVS_SE_SCISSOR_TOP, 0x00000000);
    etna_set_state_fixp(stream, VIVS_SE_SCISSOR_RIGHT, 0x07800000);
    etna_set_state_fixp(stream, VIVS_SE_SCISSOR_BOTTOM, 0x04381111);
    etna_set_state_fixp(stream, VIVS_SE_CLIP_RIGHT, 0x0780ffff);
    etna_set_state_fixp(stream, VIVS_SE_CLIP_BOTTOM, 0x0438ffff);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000002);
    etna_set_state(stream, VIVS_PE_HALTI3_UNK014BC, 0x05000000);
    etna_set_state(stream, VIVS_PE_COLOR_FORMAT, 0x06112f10);
    etna_set_state(stream, VIVS_PE_DITHER(0), 0x6e4ca280);
    etna_set_state(stream, VIVS_PE_DITHER(1), 0x5d7f91b3);
    etna_set_state_reloc(stream, VIVS_PE_PIPE_COLOR_ADDR(0), &info->ADDR_RENDER_TARGET_A);
    etna_set_state_reloc(stream, VIVS_PE_PIPE_COLOR_ADDR(1), &info->ADDR_RENDER_TARGET_A);
    etna_set_state(stream, VIVS_PE_COLOR_STRIDE, 0x00001e00);
    etna_set_state(stream, VIVS_PE_LOGIC_OP, 0x3fffffff);
    etna_set_state(stream, VIVS_PE_LOGIC_OP, 0xfffffe7f);
    etna_set_state(stream, VIVS_PE_ALPHA_OP, 0x00000070);
    etna_set_state(stream, VIVS_PE_ALPHA_BLEND_COLOR, 0x00000000);
    etna_set_state(stream, VIVS_PE_ALPHA_CONFIG, 0x00100010);
    etna_set_state(stream, VIVS_PE_HALTI4_UNK014C0, 0x00000000);
    etna_set_state(stream, VIVS_PE_ALPHA_COLOR_EXT0, 0x00000000);
    etna_set_state(stream, VIVS_PE_ALPHA_COLOR_EXT1, 0x00000000);
    etna_set_state(stream, VIVS_PE_STENCIL_CONFIG_EXT, 0x0000fdff);
    etna_set_state(stream, VIVS_PE_LOGIC_OP, 0xfffffbff);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_PE_DEPTH_CONFIG, 0x05050710);
    etna_set_state(stream, VIVS_PE_DEPTH_NEAR, 0x00000000);
    etna_set_state(stream, VIVS_PE_DEPTH_FAR, 0x3f800000);
    etna_set_state(stream, VIVS_PE_DEPTH_NORMALIZE, 0x4b7fffff);
    etna_set_state_reloc(stream, VIVS_PE_PIPE_DEPTH_ADDR(0), &info->ADDR_DEPTH_A);
    etna_set_state_reloc(stream, VIVS_PE_PIPE_DEPTH_ADDR(1), &info->ADDR_DEPTH_A);
    etna_set_state(stream, VIVS_PE_LOGIC_OP, 0xfffffe7f);
    etna_set_state(stream, VIVS_PE_DEPTH_STRIDE, 0x00001e00);
    etna_set_state(stream, VIVS_PE_LOGIC_OP, 0xff4fffff);
    etna_set_state(stream, VIVS_PE_LOGIC_OP, 0xff4fffff);
    etna_set_state(stream, VIVS_RA_EARLY_DEPTH, 0x50000031);
    etna_set_state(stream, VIVS_PE_HDEPTH_CONTROL, 0x00000000);
    etna_set_state(stream, VIVS_RA_HDEPTH_CONTROL, 0x00007000);
    etna_set_state(stream, VIVS_PA_VIEWPORT_SCALE_Z, 0x3f000000);
    etna_set_state(stream, VIVS_PA_VIEWPORT_OFFSET_Z, 0x3f000000);
    etna_set_state(stream, VIVS_PE_STENCIL_OP, 0x00070007);
    etna_set_state(stream, VIVS_PE_STENCIL_CONFIG, 0x00000000);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000003);
    etna_set_state(stream, VIVS_GL_SEMAPHORE_TOKEN, 0x00000701);
    etna_cmd_stream_emit(stream, 0x48000000); /* command STALL (9) OP=STALL */
    etna_cmd_stream_emit(stream, 0x00000701); /* command   TOKEN FROM=FE,TO=PE,UNK28=0x0 */
    etna_set_state(stream, VIVS_PE_LOGIC_OP, 0xf0ffffff);
    etna_set_state(stream, VIVS_GL_MULTI_SAMPLE_CONFIG, 0x00300000);
    etna_set_state(stream, VIVS_PA_LINE_WIDTH, 0x3f000000);
    etna_set_state(stream, VIVS_PA_CONFIG, 0x00412100);
    etna_set_state(stream, VIVS_PA_WIDE_LINE_WIDTH0, 0x3f000000);
    etna_set_state(stream, VIVS_PA_WIDE_LINE_WIDTH1, 0x3f000000);
    etna_set_state(stream, VIVS_VS_INPUT_COUNT, 0x00000103);
    etna_set_state(stream, VIVS_VS_HALTI5_UNK008E0(0), 0x00000003);
    etna_set_state(stream, VIVS_VS_HALTI5_UNK00870, 0x00002002);
    etna_set_state(stream, VIVS_VS_HALTI5_UNK008A0, 0x0881000e);
    etna_set_state(stream, VIVS_VS_TEMP_REGISTER_CONTROL, 0x00000005);
    etna_set_state(stream, VIVS_VS_HALTI5_UNK008A8, 0x00000020);
    etna_set_state(stream, VIVS_VS_UNIFORM_BASE, 0x00000000);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(44), 0x40000000);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(45), 0x40000000);
    etna_set_state(stream, VIVS_SH_HALTI5_UNIFORMS_MIRROR(46), 0x41a00000);
    etna_set_state(stream, VIVS_VS_NEWRANGE_LOW, 0x00000000);
    etna_set_state(stream, VIVS_VS_NEWRANGE_HIGH, 0x00000014);
    etna_set_state_reloc(stream, VIVS_VS_INST_ADDR, &info->ADDR_ICACHE_A);
    etna_set_state(stream, VIVS_VS_HALTI5_UNK15600, 0x00000002);
    etna_set_state(stream, VIVS_VS_ICACHE_CONTROL, 0x00000001);
    etna_set_state(stream, VIVS_VS_HALTI5_UNK15604, 0x00000013);
    etna_set_state(stream, VIVS_RA_CONTROL, 0x00000001);
    etna_set_state(stream, VIVS_PS_OUTPUT_REG, 0x00000001);
    etna_set_state(stream, VIVS_PS_UNK0102C, 0x00000000);
    etna_set_state(stream, VIVS_PS_UNK01034, 0x0000003f);
    etna_set_state(stream, VIVS_PS_UNK01038, 0x00000000);
    etna_set_state(stream, VIVS_PA_HALTI5_UNK00A90(0), 0x00000004);
    etna_set_state(stream, VIVS_PS_HALTI5_UNK01080(0), 0x00000004);
    etna_set_state(stream, VIVS_PA_HALTI5_UNK00A90(1), 0x00000000);
    etna_set_state(stream, VIVS_PS_HALTI5_UNK01080(1), 0x00000000);
    etna_set_state(stream, VIVS_PA_HALTI5_UNK00A90(2), 0x00000000);
    etna_set_state(stream, VIVS_PS_HALTI5_UNK01080(2), 0x00000000);
    etna_set_state(stream, VIVS_PA_HALTI5_UNK00A90(3), 0x00000000);
    etna_set_state(stream, VIVS_PS_HALTI5_UNK01080(3), 0x00000000);
    etna_set_state(stream, VIVS_GL_HALTI5_UNK038C0(0), 0x00000000);
    etna_set_state(stream, VIVS_GL_HALTI5_UNK038C0(1), 0x00000000);
    etna_set_state(stream, VIVS_GL_HALTI5_UNK038C0(2), 0x00000000);
    etna_set_state(stream, VIVS_GL_HALTI5_UNK038C0(3), 0x00000000);
    etna_set_state(stream, VIVS_GL_HALTI5_UNK038C0(4), 0x00000000);
    etna_set_state(stream, VIVS_GL_HALTI5_UNK038C0(5), 0x00000000);
    etna_set_state(stream, VIVS_GL_HALTI5_UNK038C0(6), 0x00000000);
    etna_set_state(stream, VIVS_GL_HALTI5_UNK038C0(7), 0x00000000);
    etna_set_state(stream, VIVS_GL_HALTI5_UNK038C0(8), 0x00000000);
    etna_set_state(stream, VIVS_GL_HALTI5_UNK038C0(9), 0x00000000);
    etna_set_state(stream, VIVS_GL_HALTI5_UNK038C0(10), 0x00000000);
    etna_set_state(stream, VIVS_GL_HALTI5_UNK038C0(11), 0x00000000);
    etna_set_state(stream, VIVS_GL_HALTI5_UNK038C0(12), 0x00000000);
    etna_set_state(stream, VIVS_GL_HALTI5_UNK038C0(13), 0x00000000);
    etna_set_state(stream, VIVS_GL_HALTI5_UNK038C0(14), 0x00000000);
    etna_set_state(stream, VIVS_GL_HALTI5_UNK038C0(15), 0x00000000);
    etna_set_state(stream, VIVS_PS_TEMP_REGISTER_CONTROL, 0x00000002);
    etna_set_state(stream, VIVS_PS_HALTI5_UNK01058, 0x00000000);
    etna_set_state(stream, VIVS_PS_UNIFORM_BASE, 0x0000013f);
    etna_set_state(stream, VIVS_PS_NEWRANGE_LOW, 0x00000000);
    etna_set_state(stream, VIVS_PS_NEWRANGE_HIGH, 0x00000001);
    etna_set_state_reloc(stream, VIVS_PS_INST_ADDR, &info->ADDR_ICACHE_B);
    etna_set_state(stream, VIVS_VS_HALTI5_UNK15600, 0x00000006);
    etna_set_state(stream, VIVS_VS_ICACHE_CONTROL, 0x00000001);
    etna_set_state(stream, VIVS_PS_HALTI5_UNK01094, 0x00000000);
    etna_set_state(stream, VIVS_PS_INPUT_COUNT, 0x00001f02);
    etna_set_state(stream, VIVS_PS_TEMP_REGISTER_CONTROL, 0x00000002);
    etna_set_state(stream, VIVS_PS_CONTROL, 0x00000002);
    etna_set_state(stream, VIVS_PA_HALTI5_UNK00AA8, 0x00000002);
    etna_set_state(stream, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT, 0x00000100);
    etna_set_state(stream, VIVS_GL_VARYING_TOTAL_COMPONENTS, 0x00000004);
    etna_set_state(stream, VIVS_PS_CONTROL_EXT, 0x00000000);
    etna_set_state(stream, VIVS_PS_UNK0102C, 0x00000000);
    etna_set_state(stream, VIVS_VS_LOAD_BALANCING, 0x0f3f022a);
    etna_set_state(stream, VIVS_VS_OUTPUT_COUNT, 0x00000002);
    etna_set_state(stream, VIVS_GL_HALTI5_UNK03888, 0x7f7f7f00);
    etna_set_state(stream, VIVS_VS_UNK0088C, 0x00000000);
    etna_set_state(stream, VIVS_PS_UNK01048, 0x00000000);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(0), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(1), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(2), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(3), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(4), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(5), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(6), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(7), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(8), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(9), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(10), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(11), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(12), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(13), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(14), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(15), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(16), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(17), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(18), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(19), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(20), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(21), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(22), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(23), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(24), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(25), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(26), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(27), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(28), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(29), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(30), &info->ADDR_TXDESC_A);
    etna_set_state_reloc(stream, VIVS_NTE_DESCRIPTOR_ADDR(31), &info->ADDR_TXDESC_A);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000000);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000001);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000002);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000003);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000004);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000005);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000006);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000007);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000008);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000009);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x2000000a);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x2000000b);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x2000000c);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x2000000d);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x2000000e);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x2000000f);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000010);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000011);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000012);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000013);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000014);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000015);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000016);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000017);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000018);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x20000019);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x2000001a);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x2000001b);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x2000001c);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x2000001d);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x2000001e);
    etna_set_state(stream, VIVS_NTE_DESCRIPTOR_INVALIDATE, 0x2000001f);
    etna_set_state(stream, VIVS_GL_SHADER_INDEX, 0x00000003);
    etna_set_state(stream, VIVS_GL_SEMAPHORE_TOKEN, 0x00000705);
    etna_set_state(stream, VIVS_GL_STALL_TOKEN, 0x00000705);
    etna_cmd_stream_emit(stream, 0x60050001); /* command DRAW_INSTANCED (12) OP=DRAW_INSTANCED,INDEXED=0,TYPE=TRIANGLE_STRIP,INSTANCE_COUNT_LO=0x1 */
    etna_cmd_stream_emit(stream, 0x00000004); /* command   COUNT INSTANCE_COUNT_HI=0x0,VERTEX_COUNT=0x4 */
    etna_cmd_stream_emit(stream, 0x00000000); /* command   START INDEX=0x0 */
    etna_cmd_stream_emit(stream, 0x00000000); /* command PAD */
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(0), 0x00003008);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(1), 0x000c3008);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(2), 0x00183008);
    etna_set_state(stream, VIVS_VS_HALTI5_UNK008C0(0), 0x00010200);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(0), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(1), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(2), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(0), 0x0000000c);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(1), 0x00000018);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(2), 0x00000824);
    etna_set_state_reloc(stream, VIVS_NFE_VERTEX_STREAMS_BASE_ADDR(0), &info->ADDR_VERTEX_A);
    etna_set_state(stream, VIVS_NFE_VERTEX_STREAMS_CONTROL(0), 0x00000024);
    etna_set_state(stream, VIVS_NFE_VERTEX_STREAMS_UNK14680(0), 0x00000000);
    etna_set_state(stream, VIVS_FE_HALTI5_UNK007C4, 0x00000000);
    etna_set_state(stream, VIVS_VS_UNK0088C, 0x00000000);
    etna_set_state(stream, VIVS_PS_UNK01048, 0x00000000);
    etna_set_state(stream, VIVS_GL_SHADER_INDEX, 0x00000003);
    etna_cmd_stream_emit(stream, 0x60050001); /* command DRAW_INSTANCED (12) OP=DRAW_INSTANCED,INDEXED=0,TYPE=TRIANGLE_STRIP,INSTANCE_COUNT_LO=0x1 */
    etna_cmd_stream_emit(stream, 0x00000004); /* command   COUNT INSTANCE_COUNT_HI=0x0,VERTEX_COUNT=0x4 */
    etna_cmd_stream_emit(stream, 0x00000004); /* command   START INDEX=0x4 */
    etna_cmd_stream_emit(stream, 0x00000000); /* command PAD */
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(0), 0x00003008);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(1), 0x000c3008);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(2), 0x00183008);
    etna_set_state(stream, VIVS_VS_HALTI5_UNK008C0(0), 0x00010200);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(0), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(1), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(2), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(0), 0x0000000c);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(1), 0x00000018);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(2), 0x00000824);
    etna_set_state_reloc(stream, VIVS_NFE_VERTEX_STREAMS_BASE_ADDR(0), &info->ADDR_VERTEX_A);
    etna_set_state(stream, VIVS_NFE_VERTEX_STREAMS_CONTROL(0), 0x00000024);
    etna_set_state(stream, VIVS_NFE_VERTEX_STREAMS_UNK14680(0), 0x00000000);
    etna_set_state(stream, VIVS_FE_HALTI5_UNK007C4, 0x00000000);
    etna_set_state(stream, VIVS_VS_UNK0088C, 0x00000000);
    etna_set_state(stream, VIVS_PS_UNK01048, 0x00000000);
    etna_set_state(stream, VIVS_GL_SHADER_INDEX, 0x00000003);
    etna_cmd_stream_emit(stream, 0x60050001); /* command DRAW_INSTANCED (12) OP=DRAW_INSTANCED,INDEXED=0,TYPE=TRIANGLE_STRIP,INSTANCE_COUNT_LO=0x1 */
    etna_cmd_stream_emit(stream, 0x00000004); /* command   COUNT INSTANCE_COUNT_HI=0x0,VERTEX_COUNT=0x4 */
    etna_cmd_stream_emit(stream, 0x00000008); /* command   START INDEX=0x8 */
    etna_cmd_stream_emit(stream, 0x00000000); /* command PAD */
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(0), 0x00003008);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(1), 0x000c3008);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(2), 0x00183008);
    etna_set_state(stream, VIVS_VS_HALTI5_UNK008C0(0), 0x00010200);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(0), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(1), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(2), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(0), 0x0000000c);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(1), 0x00000018);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(2), 0x00000824);
    etna_set_state_reloc(stream, VIVS_NFE_VERTEX_STREAMS_BASE_ADDR(0), &info->ADDR_VERTEX_A);
    etna_set_state(stream, VIVS_NFE_VERTEX_STREAMS_CONTROL(0), 0x00000024);
    etna_set_state(stream, VIVS_NFE_VERTEX_STREAMS_UNK14680(0), 0x00000000);
    etna_set_state(stream, VIVS_FE_HALTI5_UNK007C4, 0x00000000);
    etna_set_state(stream, VIVS_VS_UNK0088C, 0x00000000);
    etna_set_state(stream, VIVS_PS_UNK01048, 0x00000000);
    etna_set_state(stream, VIVS_GL_SHADER_INDEX, 0x00000003);
    etna_cmd_stream_emit(stream, 0x60050001); /* command DRAW_INSTANCED (12) OP=DRAW_INSTANCED,INDEXED=0,TYPE=TRIANGLE_STRIP,INSTANCE_COUNT_LO=0x1 */
    etna_cmd_stream_emit(stream, 0x00000004); /* command   COUNT INSTANCE_COUNT_HI=0x0,VERTEX_COUNT=0x4 */
    etna_cmd_stream_emit(stream, 0x0000000c); /* command   START INDEX=0xc */
    etna_cmd_stream_emit(stream, 0x00000000); /* command PAD */
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(0), 0x00003008);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(1), 0x000c3008);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(2), 0x00183008);
    etna_set_state(stream, VIVS_VS_HALTI5_UNK008C0(0), 0x00010200);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(0), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(1), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(2), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(0), 0x0000000c);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(1), 0x00000018);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(2), 0x00000824);
    etna_set_state_reloc(stream, VIVS_NFE_VERTEX_STREAMS_BASE_ADDR(0), &info->ADDR_VERTEX_A);
    etna_set_state(stream, VIVS_NFE_VERTEX_STREAMS_CONTROL(0), 0x00000024);
    etna_set_state(stream, VIVS_NFE_VERTEX_STREAMS_UNK14680(0), 0x00000000);
    etna_set_state(stream, VIVS_FE_HALTI5_UNK007C4, 0x00000000);
    etna_set_state(stream, VIVS_VS_UNK0088C, 0x00000000);
    etna_set_state(stream, VIVS_PS_UNK01048, 0x00000000);
    etna_set_state(stream, VIVS_GL_SHADER_INDEX, 0x00000003);
    etna_cmd_stream_emit(stream, 0x60050001); /* command DRAW_INSTANCED (12) OP=DRAW_INSTANCED,INDEXED=0,TYPE=TRIANGLE_STRIP,INSTANCE_COUNT_LO=0x1 */
    etna_cmd_stream_emit(stream, 0x00000004); /* command   COUNT INSTANCE_COUNT_HI=0x0,VERTEX_COUNT=0x4 */
    etna_cmd_stream_emit(stream, 0x00000010); /* command   START INDEX=0x10 */
    etna_cmd_stream_emit(stream, 0x00000000); /* command PAD */
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(0), 0x00003008);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(1), 0x000c3008);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG0(2), 0x00183008);
    etna_set_state(stream, VIVS_VS_HALTI5_UNK008C0(0), 0x00010200);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(0), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(1), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_SCALE(2), 0x3f800000);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(0), 0x0000000c);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(1), 0x00000018);
    etna_set_state(stream, VIVS_NFE_GENERIC_ATTRIB_CONFIG1(2), 0x00000824);
    etna_set_state_reloc(stream, VIVS_NFE_VERTEX_STREAMS_BASE_ADDR(0), &info->ADDR_VERTEX_A);
    etna_set_state(stream, VIVS_NFE_VERTEX_STREAMS_CONTROL(0), 0x00000024);
    etna_set_state(stream, VIVS_NFE_VERTEX_STREAMS_UNK14680(0), 0x00000000);
    etna_set_state(stream, VIVS_FE_HALTI5_UNK007C4, 0x00000000);
    etna_set_state(stream, VIVS_VS_UNK0088C, 0x00000000);
    etna_set_state(stream, VIVS_PS_UNK01048, 0x00000000);
    etna_set_state(stream, VIVS_GL_SHADER_INDEX, 0x00000003);
    etna_cmd_stream_emit(stream, 0x60050001); /* command DRAW_INSTANCED (12) OP=DRAW_INSTANCED,INDEXED=0,TYPE=TRIANGLE_STRIP,INSTANCE_COUNT_LO=0x1 */
    etna_cmd_stream_emit(stream, 0x00000004); /* command   COUNT INSTANCE_COUNT_HI=0x0,VERTEX_COUNT=0x4 */
    etna_cmd_stream_emit(stream, 0x00000014); /* command   START INDEX=0x14 */
    etna_cmd_stream_emit(stream, 0x00000000); /* command PAD */
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000c23);
}

void gen_cmdbuf_2(struct etna_cmd_stream *stream, struct test_info *info)
{
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000c23);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000c23);
    etna_set_state(stream, VIVS_TS_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_TS_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000c23);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000c23);
    etna_set_state(stream, VIVS_TS_FLUSH_CACHE, 0x00000001);
    etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000001);
    etna_set_state(stream, VIVS_BLT_CONFIG, 0x00000000);
    etna_set_state(stream, VIVS_BLT_SRC_STRIDE, 0x60c01e00);
    etna_set_state(stream, VIVS_BLT_SRC_CONFIG, 0x0023c833);
    etna_set_state(stream, VIVS_BLT_SWIZZLE, 0x00688688);
    etna_set_state(stream, VIVS_BLT_UNK140A0, 0x00040004);
    etna_set_state(stream, VIVS_BLT_UNK1409C, 0x00400040);
    etna_set_state_reloc(stream, VIVS_BLT_SRC_TS, &info->ADDR_TILE_STATUS_B);
    etna_set_state(stream, VIVS_BLT_SRC_TS_CLEAR_VALUE0, 0xff808080);
    etna_set_state_reloc(stream, VIVS_BLT_SRC_ADDR, &info->ADDR_RENDER_TARGET_A);
    etna_set_state(stream, VIVS_BLT_DEST_STRIDE, 0x00c01e00);
    etna_set_state(stream, VIVS_BLT_DEST_CONFIG, 0x0041c800);
    etna_set_state_reloc(stream, VIVS_BLT_DEST_ADDR, &info->ADDR_USER_A);
    etna_set_state(stream, VIVS_BLT_SRC_POS, 0x00000000);
    etna_set_state(stream, VIVS_BLT_DEST_POS, 0x00000000);
    etna_set_state(stream, VIVS_BLT_IMAGE_SIZE, 0x04380780);
    etna_set_state(stream, VIVS_BLT_UNK14058, 0xffffffff);
    etna_set_state(stream, VIVS_BLT_UNK1405C, 0xffffffff);
    etna_set_state(stream, VIVS_BLT_SET_COMMAND, 0x00000003);
    etna_set_state(stream, VIVS_BLT_COMMAND, 0x00000002);
    etna_set_state(stream, VIVS_BLT_SET_COMMAND, 0x00000003);
    etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000000);
    etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000001);
    etna_set_state(stream, VIVS_GL_SEMAPHORE_TOKEN, 0x00001001);
    etna_cmd_stream_emit(stream, 0x48000000); /* command STALL (9) OP=STALL */
    etna_cmd_stream_emit(stream, 0x00001001); /* command   TOKEN FROM=FE,TO=BLT,UNK28=0x0 */
    etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000000);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000c23);
}

void save_result(struct test_info *info)
{
    if (info->bo_bmp) {
        char *bmp = etna_bo_map(info->bo_bmp);
        bmp_dump32_ex(bmp, 1920, 1080, false, true, false, "/tmp/kube.bmp");
    }
}

int main(int argc, char **argv)
{
    struct drm_test_info *info;
    uint64_t val;

    setenv("ETNAVIV_FDR", "/tmp/out.fdr", 1);

    if ((info = drm_test_setup(argc, argv)) == NULL) {
        return 1;
    }
    if (etna_gpu_get_param(info->gpu, ETNA_GPU_MODEL, &val)) {
        fprintf(stderr, "Could not get GPU model\n");
        goto error;
    }
    if (val != 0x7000) {
        fprintf(stderr, "This only runs on GC7000\n");
        goto error;
    }

    int fbdev = 0;
    if (argc >= 3) {
        fbdev = atoi(argv[2]);
    }
    printf("Using fb: %d\n", fbdev);

    struct test_info *tinfo = test_init(info->dev, fbdev);
    assert(tinfo);

    printf("Submitting command buffer 1\n");
    gen_cmdbuf_1(info->stream, tinfo);
    printf("Finishing command buffer 1\n");
    etna_cmd_stream_finish(info->stream);

    printf("Submitting command buffer 2\n");
    gen_cmdbuf_2(info->stream, tinfo);
    printf("Finishing command buffer 2\n");
    etna_cmd_stream_finish(info->stream);

    printf("Mapping and saving result\n");
    save_result(tinfo);

    test_free(info->dev, tinfo);

    drm_test_teardown(info);
    return 0;
error:
    drm_test_teardown(info);
    return 1;
}

