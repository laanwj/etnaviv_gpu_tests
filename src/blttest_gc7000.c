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

#include <state.xml.h>
#include <state_3d.xml.h>
#include <state_blt.xml.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

struct test_info {
    struct etna_bo *bo_data;

    uint32_t ts_clear_value[2];
};

static void init_reloc(struct etna_reloc *reloc, struct etna_bo *bo, uint32_t offset, uint32_t flags)
{
    reloc->bo = bo;
    reloc->offset = offset;
    reloc->flags = flags;
}

#define DATA_SIZE (0x7f8000)

struct test_info *test_init(struct etna_device *conn)
{
    struct test_info *info = CALLOC_STRUCT(test_info);

    /* Allocate bos */
    info->bo_data = etna_bo_new(conn, DATA_SIZE, DRM_ETNA_GEM_CACHE_WC);
    assert(info->bo_data);

    etna_bo_cpu_prep(info->bo_data, DRM_ETNA_PREP_WRITE);
    memset(etna_bo_map(info->bo_data), 0, DATA_SIZE);
    etna_bo_cpu_fini(info->bo_data);

    return info;
}

void test_free(struct etna_device *conn, struct test_info *info)
{
    etna_bo_del(info->bo_data);
    free(info);
}

void emit_blt_copybuffer(struct etna_cmd_stream *stream, struct etna_reloc *dest, struct etna_reloc *src, uint32_t size)
{
    etna_cmd_stream_reserve(stream, 64*2); /* Make sure BLT op doesn't get broken up */

    etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000001);
    etna_set_state_reloc(stream, VIVS_BLT_SRC_ADDR, src);
    etna_set_state_reloc(stream, VIVS_BLT_DEST_ADDR, dest);
    etna_set_state(stream, VIVS_BLT_BUFFER_SIZE, size);
    etna_set_state(stream, VIVS_BLT_SET_COMMAND, 0x00000003);
    etna_set_state(stream, VIVS_BLT_COMMAND, VIVS_BLT_COMMAND_COMMAND_COPY_BUFFER);
    etna_set_state(stream, VIVS_BLT_SET_COMMAND, 0x00000003);
    etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000000);

    /* Synchronize FE with BLT, because we want to see result after finishing command buffer */
    etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000001);
    etna_set_state(stream, VIVS_GL_SEMAPHORE_TOKEN, 0x30001001);
    etna_cmd_stream_emit(stream, 0x48000000); /* command STALL (9) OP=STALL */
    etna_cmd_stream_emit(stream, 0x30001001); /* command   TOKEN FROM=FE,TO=BLT,UNK28=0x3 */
    etna_set_state(stream, VIVS_BLT_ENABLE, 0x00000000);
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

    uint8_t *data = etna_bo_map(tinfo->bo_data);
    const char testdata[] = "Testing buffer copy *~*~*~*~*~*~~*~**~";
    const size_t testdata_size = sizeof(testdata);

    /***********************************/
    printf("Test aligned copy "); fflush(stdout);
    etna_bo_cpu_prep(tinfo->bo_data, DRM_ETNA_PREP_WRITE);
    memcpy(data, testdata, testdata_size);
    etna_bo_cpu_fini(tinfo->bo_data);

    struct etna_reloc src, dest;
    init_reloc(&src, tinfo->bo_data, 0, ETNA_RELOC_READ);
    init_reloc(&dest, tinfo->bo_data, 0x200, ETNA_RELOC_WRITE);
    emit_blt_copybuffer(info->stream, &dest, &src, testdata_size);

    etna_cmd_stream_finish(info->stream);

    etna_bo_cpu_prep(tinfo->bo_data, DRM_ETNA_PREP_READ);
    if (memcmp(&data[0x200], testdata, testdata_size) == 0) {
        printf("OK\n");
    } else {
        printf("FAIL\n");
    }
    etna_bo_cpu_fini(tinfo->bo_data);

    /***********************************/
    printf("Test unaligned desination "); fflush(stdout);

    init_reloc(&src, tinfo->bo_data, 0, ETNA_RELOC_READ);
    init_reloc(&dest, tinfo->bo_data, 0x213, ETNA_RELOC_WRITE);
    emit_blt_copybuffer(info->stream, &dest, &src, testdata_size);

    etna_cmd_stream_finish(info->stream);

    etna_bo_cpu_prep(tinfo->bo_data, DRM_ETNA_PREP_READ);
    if (memcmp(&data[0x213], testdata, testdata_size) == 0) {
        printf("OK\n");
    } else {
        printf("FAIL\n");
    }
    etna_bo_cpu_fini(tinfo->bo_data);

    /***********************************/
    printf("Test unaligned source "); fflush(stdout);

    init_reloc(&src, tinfo->bo_data, 0x213, ETNA_RELOC_READ);
    init_reloc(&dest, tinfo->bo_data, 0x400, ETNA_RELOC_WRITE);
    emit_blt_copybuffer(info->stream, &dest, &src, testdata_size);

    etna_cmd_stream_finish(info->stream);

    etna_bo_cpu_prep(tinfo->bo_data, DRM_ETNA_PREP_READ);
    if (memcmp(&data[0x400], testdata, testdata_size) == 0) {
        printf("OK\n");
    } else {
        printf("FAIL\n");
    }
    etna_bo_cpu_fini(tinfo->bo_data);

    /***********************************/
    printf("Test unaligned source and destination "); fflush(stdout);

    init_reloc(&src, tinfo->bo_data, 0x213, ETNA_RELOC_READ);
    init_reloc(&dest, tinfo->bo_data, 0x413, ETNA_RELOC_WRITE);
    emit_blt_copybuffer(info->stream, &dest, &src, testdata_size);

    etna_cmd_stream_finish(info->stream);

    etna_bo_cpu_prep(tinfo->bo_data, DRM_ETNA_PREP_READ);
    if (memcmp(&data[0x413], testdata, testdata_size) == 0) {
        printf("OK\n");
    } else {
        printf("FAIL\n");
    }
    etna_bo_cpu_fini(tinfo->bo_data);

    test_free(info->dev, tinfo);

    drm_test_teardown(info);
    return 0;
error:
    drm_test_teardown(info);
    return 1;
}

