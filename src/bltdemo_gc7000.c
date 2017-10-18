#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 500
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "write_bmp.h"
#include "memutil.h"
#include "etnaviv_blt.h"
#include "drm_setup.h"
#include "cmdstream.h"
#include "etna_fb.h"
#include "etna_util.h"
#include "color.h"

#include "hw/common_3d.xml.h"
#include "hw/state.xml.h"
#include "hw/state_3d.xml.h"
#include "hw/state_blt.xml.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define WIDTH 1920
#define HEIGHT 1080

struct test_info {
    struct etna_bo *bo_color;
    struct etna_bo *bo_color_ts;

    struct etna_reloc ADDR_RENDER_TARGET_A;
    struct etna_reloc ADDR_TILE_STATUS_B; /* Color TS */
    struct etna_reloc ADDR_USER_A; /* Bitmap out */
    struct fb_info *fb;

    uint32_t ts_clear_value[2];
};

void init_reloc(struct etna_reloc *reloc, struct etna_bo *bo, uint32_t offset, uint32_t flags)
{
    reloc->bo = bo;
    reloc->offset = offset;
    reloc->flags = flags;
}

struct test_info *test_init(struct etna_device *conn)
{
    struct test_info *info = CALLOC_STRUCT(test_info);

    /* Allocate bos */
    info->bo_color = etna_bo_new(conn, 0x7f8000, DRM_ETNA_GEM_TYPE_RT);
    assert(info->bo_color);
    info->bo_color_ts = etna_bo_new(conn, 0x3fc0, DRM_ETNA_GEM_TYPE_TS);
    assert(info->bo_color_ts);

    /* Initialize relocs */
    init_reloc(&info->ADDR_RENDER_TARGET_A, info->bo_color, 0x0, ETNA_RELOC_READ|ETNA_RELOC_WRITE);
    init_reloc(&info->ADDR_TILE_STATUS_B, info->bo_color_ts, 0x0, ETNA_RELOC_READ|ETNA_RELOC_WRITE);

    int rv = fb_open(conn, 0, &info->fb);
    assert(rv == 0);
    /* HD or bust... */
    assert(info->fb->width == WIDTH && info->fb->height == HEIGHT && info->fb->rs_format==RS_FORMAT_A8R8G8B8);
    fb_set_buffer(info->fb, 0);
    info->ADDR_USER_A = info->fb->buffer[0];
    return info;
}

void test_free(struct etna_device *conn, struct test_info *info)
{
    etna_bo_del(info->bo_color);
    etna_bo_del(info->bo_color_ts);
    free(info);
}

/* Copy frame from render target to framebuffer */
void gen_cmdbuf_2(struct etna_cmd_stream *stream, struct test_info *info)
{
    etna_cmd_stream_reserve(stream, 64*2); /* Make sure BLT op doesn't get broken up */

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

    /* Make FE wait for BLT (to be able to sync frame swap) */
    etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000001);
    etna_set_state(stream, VIVS_GL_SEMAPHORE_TOKEN, 0x00001001);
    etna_cmd_stream_reserve(stream, 2);
    etna_cmd_stream_emit(stream, 0x48000000); /* command STALL (9) OP=STALL */
    etna_cmd_stream_emit(stream, 0x00001001); /* command   TOKEN FROM=FE,TO=BLT,UNK28=0x0 */
    etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000000);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, 0x00000c23);
}

#define NUM_BOGEYS 40
struct bogey {
    int iw;
    int ih;
    float posx;
    float posy;
    int iposx;
    int iposy;
    float dx;
    float dy;
    float color_phase;
    float size_phase;
    float intensity;
    uint32_t color;
};

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

    int frame = 0;
    struct bogey bogeys[NUM_BOGEYS];
    float phase = 0.0f;
    struct timespec tprev;
    uint32_t background = 0xff000000;

    clock_gettime(CLOCK_MONOTONIC, &tprev);
    for (unsigned i=0; i<NUM_BOGEYS; ++i) {
        struct bogey *b = &bogeys[i];
        memset(b, 0, sizeof(struct bogey));
        b->posx = random() % WIDTH;
        b->posy = random() % HEIGHT;
        b->dx = 100.0f + i*10.0f;
        b->dy = 100.0f + i*10.0f;
        b->color_phase = i * 0.005f;
        b->size_phase = i * 0.31f;
        b->intensity = ((float)i / (NUM_BOGEYS - 1) * 0.5f) + 0.5f;
    }

    while (true) {
        struct timespec tcur;
        clock_gettime(CLOCK_MONOTONIC, &tcur);

        float dt = (float)(tcur.tv_sec - tprev.tv_sec) + (float)(tcur.tv_nsec - tprev.tv_nsec)*1e-9f;

        /* Update physics */
        phase += dt;

        for (unsigned i=0; i<NUM_BOGEYS; ++i) {
            struct bogey *b = &bogeys[i];

            b->iw = (int)((sinf(phase * 0.50f + b->size_phase) + 1.0f)*200.0f) + 1;
            b->ih = (int)((cosf(phase * 0.95f + b->size_phase) + 1.0f)*200.0f) + 1;

            b->posx += b->dx * dt;
            b->posy += b->dy * dt;

            b->iw = etna_smax(b->iw, 1);
            b->ih = etna_smax(b->ih, 1);

            b->iposx = (int)b->posx;
            b->iposy = (int)b->posy;
            if (b->iposx < 0) {
                b->iposx = 0;
                b->dx = fabsf(b->dx);
            }
            if (b->iposy < 0) {
                b->iposy = 0;
                b->dy = fabs(b->dy);
            }
            if (b->iposx + b->iw > WIDTH) {
                b->iposx = WIDTH - b->iw;
                b->dx = -fabsf(b->dx);
            }
            if (b->iposy + b->ih > HEIGHT) {
                b->iposy = HEIGHT - b->ih;
                b->dy = -fabsf(b->dy);
            }

            b->color = hsv_argb(
                    fmod(phase * 0.03f + b->color_phase, 1.0f),
                    0.5f + fabsf(sinf(phase * 2.0f + b->color_phase * 10.0f)) * 0.5f,
                    b->intensity,
                    1.0f);
        }

        tinfo->ts_clear_value[0] = background;
        tinfo->ts_clear_value[1] = background;

        /* Clear framebuffer */
        struct blt_clear_op clr = {};
        clr.dest.addr = tinfo->ADDR_RENDER_TARGET_A;
        clr.dest.ts_addr = tinfo->ADDR_TILE_STATUS_B;
        clr.dest.bpp = 4;
        clr.dest.stride = 0x1e00;
        clr.dest.compressed = 1;
        clr.dest.compress_fmt = 3;
        clr.dest.tiling = ETNA_LAYOUT_SUPER_TILED;
        clr.dest.use_ts = 1;
        clr.dest.ts_clear_value[0] = tinfo->ts_clear_value[0];
        clr.dest.ts_clear_value[1] = tinfo->ts_clear_value[1];
        clr.dest.cache_mode = TS_CACHE_MODE_256;

        clr.clear_value[0] = background;
        clr.clear_value[1] = background;
        clr.clear_bits[0] = 0xffffffff;
        clr.clear_bits[1] = 0xffffffff;
        clr.rect_x = 0;
        clr.rect_y = 0;
        clr.rect_w = WIDTH;
        clr.rect_h = HEIGHT;
        emit_blt_clearimage(info->stream, &clr);

        /* Draw bouncing squares */
        for (unsigned i=0; i<NUM_BOGEYS; ++i) {
            struct blt_clear_op clr = {};
            struct bogey *b = &bogeys[i];
            clr.dest.addr = tinfo->ADDR_RENDER_TARGET_A;
            clr.dest.ts_addr = tinfo->ADDR_TILE_STATUS_B;
            clr.dest.bpp = 4;
            clr.dest.stride = 0x1e00;
            clr.dest.compressed = 1;
            clr.dest.compress_fmt = 3;
            clr.dest.tiling = ETNA_LAYOUT_SUPER_TILED;
            clr.dest.use_ts = 1;
            clr.dest.ts_clear_value[0] = tinfo->ts_clear_value[0];
            clr.dest.ts_clear_value[1] = tinfo->ts_clear_value[1];
            clr.dest.cache_mode = TS_CACHE_MODE_256;

            clr.clear_value[0] = clr.clear_value[1] = b->color;
            clr.clear_bits[0] = 0xffffffff; clr.clear_bits[1] = 0xffffffff;
            clr.rect_x = b->iposx;
            clr.rect_y = b->iposy;
            clr.rect_w = b->iw;
            clr.rect_h = b->ih;
            assert(clr.rect_x < WIDTH);
            assert(clr.rect_y < HEIGHT);
            assert(clr.rect_w > 0);
            assert(clr.rect_h > 0);
            assert((clr.rect_x + clr.rect_w) <= WIDTH);
            assert((clr.rect_y + clr.rect_h) <= HEIGHT);

            emit_blt_clearimage(info->stream, &clr);
        }

        /* Copy framebuffer to screen */
        gen_cmdbuf_2(info->stream, tinfo);

        etna_cmd_stream_flush(info->stream);

        frame += 1;
        tprev = tcur;

        if ((frame % 1024)==0) {
            printf("FPS %.1f. %.1f ms per frame\n", frame / phase, phase / frame*1000.0f);
        }
    }

    test_free(info->dev, tinfo);

    drm_test_teardown(info);
    return 0;
error:
    drm_test_teardown(info);
    return 1;
}

