#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 500
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "write_bmp.h"
#include "memutil.h"
#include "drm_setup.h"
#include "cmdstream.h"
#include "etna_util.h"
#include "color.h"
#include "etnaviv_tiling.h"
#include "etnaviv_blt.h"

#include "hw/state.xml.h"
#include "hw/common_3d.xml.h"
#include "hw/state_3d.xml.h"
#include "hw/state_blt.xml.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* Scratchpad size */
#define DATA_SIZE (0x7f8000)

#define WIDTH 1920
#define HEIGHT 1080

struct test_info {
    struct etna_bo *bo_color;
    struct etna_bo *bo_color_ts;
    struct etna_bo *bo_data;
    struct etna_bo *bo_bmp;

    struct etna_reloc ADDR_RENDER_TARGET_A;
    struct etna_reloc ADDR_TILE_STATUS_B; /* Color TS */
    struct etna_reloc ADDR_USER_A; /* Bitmap out */
    struct fb_info *fb;

    bool use_ts;
    uint32_t ts_clear_value[2];
};

static void init_reloc_rw(struct etna_reloc *reloc, struct etna_bo *bo, uint32_t offset)
{
    reloc->bo = bo;
    reloc->offset = offset;
    reloc->flags = ETNA_RELOC_READ|ETNA_RELOC_WRITE;
}
static void init_reloc_r(struct etna_reloc *reloc, struct etna_bo *bo, uint32_t offset)
{
    reloc->bo = bo;
    reloc->offset = offset;
    reloc->flags = ETNA_RELOC_READ;
}
static void init_reloc_w(struct etna_reloc *reloc, struct etna_bo *bo, uint32_t offset)
{
    reloc->bo = bo;
    reloc->offset = offset;
    reloc->flags = ETNA_RELOC_WRITE;
}

struct test_info *test_init(struct etna_device *conn)
{
    struct test_info *info = CALLOC_STRUCT(test_info);

    info->bo_color = etna_bo_new(conn, 0x7f8000, DRM_ETNA_GEM_TYPE_RT);
    assert(info->bo_color);
    /* 1 bit per tile? */
    info->bo_color_ts = etna_bo_new(conn, 0x3fc0, DRM_ETNA_GEM_TYPE_TS);
    assert(info->bo_color_ts);
    info->bo_data = etna_bo_new(conn, DATA_SIZE, DRM_ETNA_GEM_CACHE_WC);
    assert(info->bo_data);
    info->bo_bmp = etna_bo_new(conn, 0x7e9000, DRM_ETNA_GEM_TYPE_BMP);
    assert(info->bo_bmp);

    init_reloc_rw(&info->ADDR_RENDER_TARGET_A, info->bo_color, 0x0);
    init_reloc_rw(&info->ADDR_TILE_STATUS_B, info->bo_color_ts, 0x0);
    init_reloc_rw(&info->ADDR_USER_A, info->bo_bmp, 0x0);

    /* empty sandbox */
    etna_bo_cpu_prep(info->bo_data, DRM_ETNA_PREP_WRITE);
    memset(etna_bo_map(info->bo_data), 0, DATA_SIZE);
    etna_bo_cpu_fini(info->bo_data);

    etna_bo_cpu_prep(info->bo_color, DRM_ETNA_PREP_WRITE);
    memset(etna_bo_map(info->bo_color), 0, 0x7f8000);
    etna_bo_cpu_fini(info->bo_color);

    etna_bo_cpu_prep(info->bo_bmp, DRM_ETNA_PREP_WRITE);
    memset(etna_bo_map(info->bo_bmp), 0, 0x7e9000);
    etna_bo_cpu_fini(info->bo_bmp);
    return info;
}

void test_free(struct etna_device *conn, struct test_info *info)
{
    etna_bo_del(info->bo_data);
    etna_bo_del(info->bo_color);
    etna_bo_del(info->bo_color_ts);
    free(info);
}

/* Full screen clear */
void gen_cmdbuf_clear(struct etna_cmd_stream *stream, struct test_info *info)
{
    etna_cmd_stream_reserve(stream, 64*2); /* Never allow BLT sequences to be broken up */
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
    etna_set_state(stream, VIVS_BLT_CLEAR_COLOR0, info->ts_clear_value[0]);
    etna_set_state(stream, VIVS_BLT_CLEAR_COLOR1, info->ts_clear_value[1]);
    etna_set_state(stream, VIVS_BLT_CLEAR_BITS0, 0xffffffff);
    etna_set_state(stream, VIVS_BLT_CLEAR_BITS1, 0xffffffff);
    etna_set_state_reloc(stream, VIVS_BLT_DEST_TS, &info->ADDR_TILE_STATUS_B);
    etna_set_state_reloc(stream, VIVS_BLT_SRC_TS, &info->ADDR_TILE_STATUS_B);
    etna_set_state(stream, VIVS_BLT_DEST_TS_CLEAR_VALUE0, info->ts_clear_value[0]);
    etna_set_state(stream, VIVS_BLT_DEST_TS_CLEAR_VALUE1, info->ts_clear_value[1]);
    etna_set_state(stream, VIVS_BLT_SRC_TS_CLEAR_VALUE0, info->ts_clear_value[0]);
    etna_set_state(stream, VIVS_BLT_SRC_TS_CLEAR_VALUE1, info->ts_clear_value[1]);
    etna_set_state(stream, VIVS_BLT_SET_COMMAND, 0x00000003);
    etna_set_state(stream, VIVS_BLT_COMMAND, VIVS_BLT_COMMAND_COMMAND_CLEAR_IMAGE);
    etna_set_state(stream, VIVS_BLT_SET_COMMAND, 0x00000003);
    etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000000);
}

/* Full screen copy */
void gen_cmdbuf_framebuffer(struct etna_cmd_stream *stream, struct test_info *info)
{
    etna_cmd_stream_reserve(stream, 64*2); /* Never allow BLT sequences to be broken up */
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000c23);
    etna_set_state(stream, VIVS_TS_FLUSH_CACHE, 0x00000001);

    etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000001);
    etna_set_state(stream, VIVS_BLT_CONFIG, 0x00000000);
    etna_set_state(stream, VIVS_BLT_SRC_STRIDE, 0x60c01e00);
    etna_set_state(stream, VIVS_BLT_SRC_CONFIG, 0x0023c832 | info->use_ts);
    etna_set_state(stream, VIVS_BLT_SWIZZLE, 0x00688688);
    etna_set_state(stream, VIVS_BLT_UNK140A0, 0x00040004);
    etna_set_state(stream, VIVS_BLT_UNK1409C, 0x00400040);
    etna_set_state_reloc(stream, VIVS_BLT_SRC_TS, &info->ADDR_TILE_STATUS_B);
    etna_set_state(stream, VIVS_BLT_SRC_TS_CLEAR_VALUE0, info->ts_clear_value[0]);
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
    etna_set_state(stream, VIVS_BLT_COMMAND, VIVS_BLT_COMMAND_COMMAND_COPY_IMAGE);
    etna_set_state(stream, VIVS_BLT_SET_COMMAND, 0x00000003);
    etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000000);
}

/* Create a demo texture */
void create_linear_argb_image(uint32_t *dest, unsigned width, unsigned height, unsigned stride)
{
    assert(!(stride&3));
    stride >>= 2;

    for (unsigned y=0; y<height; ++y) {
        for (unsigned x=0; x<height; ++x) {
            dest[y*stride + x] = hsv_argb(
                    (float)x / (float)(width-1),
                    (float)y / (float)(height-1),
                    ((x & 16) | (y & 16)) ? 0.5f : 1.0f,
                    1.0f);
        }
    }
}

void save_result(struct test_info *info, const char *filename)
{
    if (info->bo_bmp) {
        char *bmp = etna_bo_map(info->bo_bmp);
        bmp_dump32_ex(bmp, 1920, 1080, true, true, true, filename);
    }
}


int main(int argc, char **argv)
{
    struct drm_test_info *info;
    uint64_t val;

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

    struct test_info *tinfo = test_init(info->dev);
    assert(tinfo);

    tinfo->use_ts = 0;
    tinfo->ts_clear_value[0] = 0xff202020;
    tinfo->ts_clear_value[1] = 0xff202020;

    uint8_t *data = etna_bo_map(tinfo->bo_data);

    gen_cmdbuf_clear(info->stream, tinfo);

    if (!tinfo->use_ts) { /* Don't use TS, do a resolve in place to get rid of it */
        struct blt_inplace_op op = {};
        init_reloc_rw(&op.addr, tinfo->bo_color, 0);
        init_reloc_rw(&op.ts_addr, tinfo->bo_color_ts, 0);
        op.ts_clear_value[0] = tinfo->ts_clear_value[0];
        op.ts_clear_value[1] = tinfo->ts_clear_value[1];
        op.num_tiles = 0x7f8000 / 256; /* 256 bytes per tile */
        emit_blt_inplace(info->stream, &op);
    }

    etna_bo_cpu_prep(tinfo->bo_data, DRM_ETNA_PREP_WRITE | DRM_ETNA_PREP_READ);
    uint32_t linear_img_ofs = 0x0;
    uint32_t tiled_img_ofs = 0x40000;
    uint32_t mip_img_ofs = 0x80000;
    uint32_t mip_level_stride = 0x40000;
    create_linear_argb_image((uint32_t*)data + linear_img_ofs, 256, 256, 256*4);
    etna_texture_tile(data + tiled_img_ofs, data, 0, 0, 256*4, 256, 256, 256*4, 4);
    etna_bo_cpu_fini(tinfo->bo_data);

    {
        struct blt_imgcopy_op op = {};

        init_reloc_r(&op.src.addr, tinfo->bo_data, linear_img_ofs);
        op.src.format = BLT_FORMAT_A8R8G8B8;
        op.src.stride = 256*4;
        op.src.tiling = ETNA_LAYOUT_LINEAR;
        op.src.cache_mode = 0;
        op.src.swizzle[0] = TEXTURE_SWIZZLE_RED;
        op.src.swizzle[1] = TEXTURE_SWIZZLE_GREEN;
        op.src.swizzle[2] = TEXTURE_SWIZZLE_BLUE;
        op.src.swizzle[3] = TEXTURE_SWIZZLE_ALPHA;

        init_reloc_w(&op.dest.addr, tinfo->bo_color, 0);
        init_reloc_w(&op.dest.ts_addr, tinfo->bo_color_ts, 0);
        op.dest.format = BLT_FORMAT_A8R8G8B8;
        op.dest.stride = 0x1e00;
        op.dest.compressed = 1;
        op.dest.compress_fmt = 3;
        op.dest.tiling = ETNA_LAYOUT_SUPER_TILED;
        op.dest.cache_mode = 1;
        op.dest.use_ts = 1;
        op.dest.ts_clear_value[0] = tinfo->ts_clear_value[0];
        op.dest.ts_clear_value[1] = tinfo->ts_clear_value[1];
        op.dest.swizzle[0] = TEXTURE_SWIZZLE_RED;
        op.dest.swizzle[1] = TEXTURE_SWIZZLE_GREEN;
        op.dest.swizzle[2] = TEXTURE_SWIZZLE_BLUE;
        op.dest.swizzle[3] = TEXTURE_SWIZZLE_ALPHA;

        /* From linear image */
        op.src_x = 0;
        op.src_y = 0;
        op.dest_x = 0;
        op.dest_y = 0;
        op.rect_w = 256;
        op.rect_h = 256;
        emit_blt_copyimage(info->stream, &op);

        op.dest_x = 300;
        op.dest_y = 3;
        emit_blt_copyimage(info->stream, &op);

        op.dest_x = 0;
        op.dest_y = 300;
        op.src_x = 0;
        op.src_y = 0;
        op.rect_w = 128;
        op.rect_h = 128;
        emit_blt_copyimage(info->stream, &op);

        op.dest_x = 176;
        op.dest_y = 300;
        op.src_x = 128;
        op.src_y = 0;
        emit_blt_copyimage(info->stream, &op);

        op.dest_x = 0;
        op.dest_y = 300+176;
        op.src_x = 0;
        op.src_y = 128;
        emit_blt_copyimage(info->stream, &op);

        op.dest_x = 176;
        op.dest_y = 300+176;
        op.src_x = 128;
        op.src_y = 128;
        emit_blt_copyimage(info->stream, &op);

        /* Now from a tiled image */
        init_reloc_r(&op.src.addr, tinfo->bo_data, tiled_img_ofs);
        op.src.format = BLT_FORMAT_A8R8G8B8;
        op.src.tiling = ETNA_LAYOUT_TILED;

        op.src_x = 0;
        op.src_y = 0;
        op.dest_x = 600;
        op.dest_y = 0;
        op.rect_w = 256;
        op.rect_h = 256;
        emit_blt_copyimage(info->stream, &op);

        op.dest_x = 600+300;
        op.dest_y = 3;
        emit_blt_copyimage(info->stream, &op);

        op.dest_x = 600+0;
        op.dest_y = 300;
        op.src_x = 0;
        op.src_y = 0;
        op.rect_w = 128;
        op.rect_h = 128;
        emit_blt_copyimage(info->stream, &op);

        op.dest_x = 600+176;
        op.dest_y = 300;
        op.src_x = 128;
        op.src_y = 0;
        emit_blt_copyimage(info->stream, &op);

        op.dest_x = 600+0;
        op.dest_y = 300+176;
        op.src_x = 0;
        op.src_y = 128;
        emit_blt_copyimage(info->stream, &op);

        op.dest_x = 600+176;
        op.dest_y = 300+176;
        op.src_x = 128;
        op.src_y = 128;
        emit_blt_copyimage(info->stream, &op);

    }

    {
        struct blt_genmipmaps_op op = {};

        init_reloc_r(&op.src.addr, tinfo->bo_data, tiled_img_ofs);
        op.src.format = BLT_FORMAT_A8R8G8B8;
        op.src.stride = 256*4;
        op.src.tiling = ETNA_LAYOUT_TILED;
        op.src.cache_mode = 0;
        op.src.swizzle[0] = TEXTURE_SWIZZLE_RED;
        op.src.swizzle[1] = TEXTURE_SWIZZLE_GREEN;
        op.src.swizzle[2] = TEXTURE_SWIZZLE_BLUE;
        op.src.swizzle[3] = TEXTURE_SWIZZLE_ALPHA;

        op.dest.format = BLT_FORMAT_A8R8G8B8;
        op.dest.stride = 256*4;
        op.dest.tiling = ETNA_LAYOUT_SUPER_TILED;
        op.dest.cache_mode = 0;
        op.dest.swizzle[0] = TEXTURE_SWIZZLE_RED;
        op.dest.swizzle[1] = TEXTURE_SWIZZLE_GREEN;
        op.dest.swizzle[2] = TEXTURE_SWIZZLE_BLUE;
        op.dest.swizzle[3] = TEXTURE_SWIZZLE_ALPHA;

        op.rect_w = 256;
        op.rect_h = 256;
        op.num_mips = 8; /* All the way from 256x256 to 1x1 */
        for (unsigned level=0; level<8; ++level) {
            init_reloc_w(&op.mip_addr[level], tinfo->bo_data, mip_img_ofs + mip_level_stride * level);
            op.mip_stride[level] = 256*4;
        }
        emit_blt_genmipmaps(info->stream, &op);
    }

    unsigned xx = 10;
    unsigned width = 128;
    unsigned height = 128;
    for (unsigned level=0; level<8; ++level) {
        struct blt_imgcopy_op op = {};

        init_reloc_r(&op.src.addr, tinfo->bo_data, mip_img_ofs + mip_level_stride * level);
        op.src.format = BLT_FORMAT_A8R8G8B8;
        op.src.stride = 256*4;
        op.src.tiling = ETNA_LAYOUT_SUPER_TILED;
        op.src.cache_mode = 0;
        op.src.swizzle[0] = TEXTURE_SWIZZLE_RED;
        op.src.swizzle[1] = TEXTURE_SWIZZLE_GREEN;
        op.src.swizzle[2] = TEXTURE_SWIZZLE_BLUE;
        op.src.swizzle[3] = TEXTURE_SWIZZLE_ALPHA;

        init_reloc_w(&op.dest.addr, tinfo->bo_color, 0);
        init_reloc_w(&op.dest.ts_addr, tinfo->bo_color_ts, 0);
        op.dest.format = BLT_FORMAT_A8R8G8B8;
        op.dest.stride = 0x1e00;
        op.dest.compressed = 1;
        op.dest.compress_fmt = 3;
        op.dest.tiling = ETNA_LAYOUT_SUPER_TILED;
        op.dest.cache_mode = 1;
        op.dest.use_ts = 1;
        op.dest.ts_clear_value[0] = tinfo->ts_clear_value[0];
        op.dest.ts_clear_value[1] = tinfo->ts_clear_value[1];
        op.dest.swizzle[0] = TEXTURE_SWIZZLE_RED;
        op.dest.swizzle[1] = TEXTURE_SWIZZLE_GREEN;
        op.dest.swizzle[2] = TEXTURE_SWIZZLE_BLUE;
        op.dest.swizzle[3] = TEXTURE_SWIZZLE_ALPHA;

        /* From linear image */
        op.src_x = 0;
        op.src_y = 0;
        op.dest_x = xx;
        op.dest_y = 700;
        op.rect_w = width;
        op.rect_h = height;
        emit_blt_copyimage(info->stream, &op);

        xx += width + 10;
        width = (width+1)/2;
        height = (height+1)/2;
    }

    gen_cmdbuf_framebuffer(info->stream, tinfo);

    emit_blt_sync_fe(info->stream);

    etna_cmd_stream_finish(info->stream);

    const char *filename = "/tmp/blttest2.bmp";
    printf("Mapping and saving result to %s\n", filename);
    save_result(tinfo, filename);

    test_free(info->dev, tinfo);

    drm_test_teardown(info);
    return 0;
error:
    drm_test_teardown(info);
    return 1;
}

