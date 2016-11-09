/*
 * Copyright (C) 2016 Etnaviv Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Wladimir J. van der Laan <laanwj@gmail.com>
 */
/* Basic "hello world" test */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <xf86drm.h>
#include <etnaviv_drmif.h>

#include "state.xml.h"
#include "state_3d.xml.h"
#include "common.xml.h"
#include "cmdstream.xml.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static inline void etna_emit_load_state(struct etna_cmd_stream *stream,
        const uint16_t offset, const uint16_t count)
{
    uint32_t v;

    v =     (VIV_FE_LOAD_STATE_HEADER_OP_LOAD_STATE | VIV_FE_LOAD_STATE_HEADER_OFFSET(offset) |
            (VIV_FE_LOAD_STATE_HEADER_COUNT(count) & VIV_FE_LOAD_STATE_HEADER_COUNT__MASK));

    etna_cmd_stream_emit(stream, v);
}

static inline void etna_set_state(struct etna_cmd_stream *stream, uint32_t address, uint32_t value)
{
    etna_cmd_stream_reserve(stream, 2);
    etna_emit_load_state(stream, address >> 2, 1);
    etna_cmd_stream_emit(stream, value);
}

static inline void etna_set_state_from_bo(struct etna_cmd_stream *stream,
        uint32_t address, struct etna_bo *bo)
{
    etna_cmd_stream_reserve(stream, 2);
    etna_emit_load_state(stream, address >> 2, 1);

    etna_cmd_stream_reloc(stream, &(struct etna_reloc){
        .bo = bo,
        .flags = ETNA_RELOC_WRITE,
        .offset = 0,
    });
}

static void gen_cmd_stream(struct etna_cmd_stream *stream, struct etna_bo *bmp)
{
    etna_set_state(stream, VIVS_PA_SYSTEM_MODE, VIVS_PA_SYSTEM_MODE_UNK0 | VIVS_PA_SYSTEM_MODE_UNK4);
    etna_set_state(stream, VIVS_GL_API_MODE, VIVS_GL_API_MODE_OPENCL);
    etna_set_state(stream, VIVS_VS_INPUT_COUNT, VIVS_VS_INPUT_COUNT_COUNT(1) | VIVS_VS_INPUT_COUNT_UNK8(31));
    etna_set_state(stream, VIVS_VS_INPUT(0), VIVS_VS_INPUT_I0(0) | VIVS_VS_INPUT_I1(1) | VIVS_VS_INPUT_I2(2) | VIVS_VS_INPUT_I3(3));
    etna_set_state(stream, VIVS_VS_TEMP_REGISTER_CONTROL, VIVS_VS_TEMP_REGISTER_CONTROL_NUM_TEMPS(10));
    etna_set_state(stream, VIVS_VS_OUTPUT_COUNT, 0);
    etna_set_state(stream, VIVS_CL_UNK00924, 0x0);

    etna_set_state_from_bo(stream, VIVS_VS_UNIFORMS(0), bmp);

    etna_set_state(stream, VIVS_GL_SEMAPHORE_TOKEN, VIVS_GL_SEMAPHORE_TOKEN_FROM(SYNC_RECIPIENT_FE) | VIVS_GL_SEMAPHORE_TOKEN_TO(SYNC_RECIPIENT_PE));
    etna_set_state(stream, VIVS_VS_INPUT_COUNT, VIVS_VS_INPUT_COUNT_COUNT(1) | VIVS_VS_INPUT_COUNT_UNK8(1));
    etna_set_state(stream, VIVS_VS_TEMP_REGISTER_CONTROL, VIVS_VS_TEMP_REGISTER_CONTROL_NUM_TEMPS(10));
    etna_set_state(stream, VIVS_VS_OUTPUT(0), VIVS_VS_OUTPUT_O0(0) | VIVS_VS_OUTPUT_O1(0) | VIVS_VS_OUTPUT_O2(0) | VIVS_VS_OUTPUT_O3(0));
    etna_set_state(stream, VIVS_GL_VARYING_NUM_COMPONENTS, VIVS_GL_VARYING_NUM_COMPONENTS_VAR0(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR1(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR2(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR3(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR4(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR5(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR6(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR7(0x0));
    etna_set_state(stream, VIVS_GL_UNK03834, 0x0);
    etna_set_state(stream, VIVS_VS_NEW_UNK00860, 0x0);
    etna_set_state(stream, VIVS_VS_RANGE, VIVS_VS_RANGE_LOW(0x0) | VIVS_VS_RANGE_HIGH(0xf));

    etna_set_state(stream, VIVS_VS_UNIFORMS(28), 0xd);
    etna_set_state(stream, VIVS_VS_UNIFORMS(29), 0x0);
    etna_set_state(stream, VIVS_VS_UNIFORMS(26), 0xc);
    etna_set_state(stream, VIVS_VS_UNIFORMS(27), 0x21);
    etna_set_state(stream, VIVS_VS_UNIFORMS(24), 0xb);
    etna_set_state(stream, VIVS_VS_UNIFORMS(25), 0x64);
    etna_set_state(stream, VIVS_VS_UNIFORMS(22), 0xa);
    etna_set_state(stream, VIVS_VS_UNIFORMS(23), 0x6c);
    etna_set_state(stream, VIVS_VS_UNIFORMS(20), 0x9);
    etna_set_state(stream, VIVS_VS_UNIFORMS(21), 0x72);
    etna_set_state(stream, VIVS_VS_UNIFORMS(18), 0x8);
    etna_set_state(stream, VIVS_VS_UNIFORMS(19), 0x6f);
    etna_set_state(stream, VIVS_VS_UNIFORMS(16), 0x7);
    etna_set_state(stream, VIVS_VS_UNIFORMS(17), 0x57);
    etna_set_state(stream, VIVS_VS_UNIFORMS(14), 0x6);
    etna_set_state(stream, VIVS_VS_UNIFORMS(15), 0x20);
    etna_set_state(stream, VIVS_VS_UNIFORMS(12), 0x5);
    etna_set_state(stream, VIVS_VS_UNIFORMS(13), 0x2c);
    etna_set_state(stream, VIVS_VS_UNIFORMS(10), 0x4);
    etna_set_state(stream, VIVS_VS_UNIFORMS(11), 0x6f);
    etna_set_state(stream, VIVS_VS_UNIFORMS(8), 0x3);
    etna_set_state(stream, VIVS_VS_UNIFORMS(9), 0x6c);
    etna_set_state(stream, VIVS_VS_UNIFORMS(6), 0x2);
    etna_set_state(stream, VIVS_VS_UNIFORMS(7), 0x6c);
    etna_set_state(stream, VIVS_VS_UNIFORMS(4), 0x1);
    etna_set_state(stream, VIVS_VS_UNIFORMS(5), 0x65);
    etna_set_state(stream, VIVS_VS_UNIFORMS(2), 0x0);
    etna_set_state(stream, VIVS_VS_UNIFORMS(1), 0x48);

    etna_set_state(stream, VIVS_SH_INST_MEM(0), 0x801009);
    etna_set_state(stream, VIVS_SH_INST_MEM(1), 0x0);
    etna_set_state(stream, VIVS_SH_INST_MEM(2), 0x0);
    etna_set_state(stream, VIVS_SH_INST_MEM(3), 0x20000008);
    etna_set_state(stream, VIVS_SH_INST_MEM(4), 0x1001009);
    etna_set_state(stream, VIVS_SH_INST_MEM(5), 0x0);
    etna_set_state(stream, VIVS_SH_INST_MEM(6), 0x0);
    etna_set_state(stream, VIVS_SH_INST_MEM(7), 0x8);
    etna_set_state(stream, VIVS_SH_INST_MEM(8), 0x800033);
    etna_set_state(stream, VIVS_SH_INST_MEM(9), 0x15400800);
    etna_set_state(stream, VIVS_SH_INST_MEM(10), 0x81540040);
    etna_set_state(stream, VIVS_SH_INST_MEM(11), 0x2015400a);
    etna_set_state(stream, VIVS_SH_INST_MEM(12), 0x800033);
    etna_set_state(stream, VIVS_SH_INST_MEM(13), 0x800);
    etna_set_state(stream, VIVS_SH_INST_MEM(14), 0x800000c0);
    etna_set_state(stream, VIVS_SH_INST_MEM(15), 0x2015401a);
    etna_set_state(stream, VIVS_SH_INST_MEM(16), 0x800033);
    etna_set_state(stream, VIVS_SH_INST_MEM(17), 0x800);
    etna_set_state(stream, VIVS_SH_INST_MEM(18), 0x815400c0);
    etna_set_state(stream, VIVS_SH_INST_MEM(19), 0x203fc01a);
    etna_set_state(stream, VIVS_SH_INST_MEM(20), 0x800033);
    etna_set_state(stream, VIVS_SH_INST_MEM(21), 0x800);
    etna_set_state(stream, VIVS_SH_INST_MEM(22), 0x80000140);
    etna_set_state(stream, VIVS_SH_INST_MEM(23), 0x2015402a);
    etna_set_state(stream, VIVS_SH_INST_MEM(24), 0x800033);
    etna_set_state(stream, VIVS_SH_INST_MEM(25), 0x800);
    etna_set_state(stream, VIVS_SH_INST_MEM(26), 0x81540140);
    etna_set_state(stream, VIVS_SH_INST_MEM(27), 0x203fc02a);
    etna_set_state(stream, VIVS_SH_INST_MEM(28), 0x800033);
    etna_set_state(stream, VIVS_SH_INST_MEM(29), 0x800);
    etna_set_state(stream, VIVS_SH_INST_MEM(30), 0x800001c0);
    etna_set_state(stream, VIVS_SH_INST_MEM(31), 0x2015403a);
    etna_set_state(stream, VIVS_SH_INST_MEM(32), 0x800033);
    etna_set_state(stream, VIVS_SH_INST_MEM(33), 0x800);
    etna_set_state(stream, VIVS_SH_INST_MEM(34), 0x815401c0);
    etna_set_state(stream, VIVS_SH_INST_MEM(35), 0x203fc03a);
    etna_set_state(stream, VIVS_SH_INST_MEM(36), 0x800033);
    etna_set_state(stream, VIVS_SH_INST_MEM(37), 0x800);
    etna_set_state(stream, VIVS_SH_INST_MEM(38), 0x80000240);
    etna_set_state(stream, VIVS_SH_INST_MEM(39), 0x2015404a);
    etna_set_state(stream, VIVS_SH_INST_MEM(40), 0x800033);
    etna_set_state(stream, VIVS_SH_INST_MEM(41), 0x800);
    etna_set_state(stream, VIVS_SH_INST_MEM(42), 0x81540240);
    etna_set_state(stream, VIVS_SH_INST_MEM(43), 0x203fc04a);
    etna_set_state(stream, VIVS_SH_INST_MEM(44), 0x800033);
    etna_set_state(stream, VIVS_SH_INST_MEM(45), 0x800);
    etna_set_state(stream, VIVS_SH_INST_MEM(46), 0x800002c0);
    etna_set_state(stream, VIVS_SH_INST_MEM(47), 0x2015405a);
    etna_set_state(stream, VIVS_SH_INST_MEM(48), 0x800033);
    etna_set_state(stream, VIVS_SH_INST_MEM(49), 0x800);
    etna_set_state(stream, VIVS_SH_INST_MEM(50), 0x815402c0);
    etna_set_state(stream, VIVS_SH_INST_MEM(51), 0x203fc05a);
    etna_set_state(stream, VIVS_SH_INST_MEM(52), 0x800033);
    etna_set_state(stream, VIVS_SH_INST_MEM(53), 0x800);
    etna_set_state(stream, VIVS_SH_INST_MEM(54), 0x80000340);
    etna_set_state(stream, VIVS_SH_INST_MEM(55), 0x2015406a);
    etna_set_state(stream, VIVS_SH_INST_MEM(56), 0x800033);
    etna_set_state(stream, VIVS_SH_INST_MEM(57), 0x800);
    etna_set_state(stream, VIVS_SH_INST_MEM(58), 0x81540340);
    etna_set_state(stream, VIVS_SH_INST_MEM(59), 0x203fc06a);
    etna_set_state(stream, VIVS_SH_INST_MEM(60), 0x800033);
    etna_set_state(stream, VIVS_SH_INST_MEM(61), 0x800);
    etna_set_state(stream, VIVS_SH_INST_MEM(62), 0x800003c0);
    etna_set_state(stream, VIVS_SH_INST_MEM(63), 0x2015407a);
    etna_set_state(stream, VIVS_SH_INST_MEM(64), 0x0);
    etna_set_state(stream, VIVS_SH_INST_MEM(65), 0x0);
    etna_set_state(stream, VIVS_SH_INST_MEM(66), 0x0);
    etna_set_state(stream, VIVS_SH_INST_MEM(67), 0x0);

    etna_set_state(stream, VIVS_PS_INPUT_COUNT, VIVS_PS_INPUT_COUNT_COUNT(1) | VIVS_PS_INPUT_COUNT_UNK8(31));
    etna_set_state(stream, VIVS_PS_TEMP_REGISTER_CONTROL, VIVS_PS_TEMP_REGISTER_CONTROL_NUM_TEMPS(10));
    etna_set_state(stream, VIVS_PS_CONTROL, 0);
    etna_set_state(stream, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT_UNK0(0x0) | VIVS_PA_ATTRIBUTE_ELEMENT_COUNT_COUNT(0x0));
    etna_set_state(stream, VIVS_GL_VARYING_TOTAL_COMPONENTS, VIVS_GL_VARYING_TOTAL_COMPONENTS_NUM(0x0));
    etna_set_state(stream, VIVS_PS_UNK01030, 0x0);
    etna_set_state(stream, VIVS_VS_LOAD_BALANCING, VIVS_VS_LOAD_BALANCING_A(0x42) | VIVS_VS_LOAD_BALANCING_B(0x5) | VIVS_VS_LOAD_BALANCING_C(0x3f) | VIVS_VS_LOAD_BALANCING_D(0xf));
    etna_set_state(stream, VIVS_VS_OUTPUT_COUNT, 1);
    etna_set_state(stream, VIVS_CL_CONFIG, VIVS_CL_CONFIG_DIMENSIONS(0x1) | VIVS_CL_CONFIG_TRAVERSE_ORDER(0x0) | VIVS_CL_CONFIG_SWATH_SIZE_X(0x0) | VIVS_CL_CONFIG_SWATH_SIZE_Y(0x0) | VIVS_CL_CONFIG_SWATH_SIZE_Z(0x0) | VIVS_CL_CONFIG_VALUE_ORDER(0x3));
    etna_set_state(stream, VIVS_CL_GLOBAL_X, VIVS_CL_GLOBAL_X_SIZE(0x1) | VIVS_CL_GLOBAL_X_OFFSET(0x0));
    etna_set_state(stream, VIVS_CL_GLOBAL_Y, VIVS_CL_GLOBAL_Y_SIZE(0x0) | VIVS_CL_GLOBAL_Y_OFFSET(0x0));
    etna_set_state(stream, VIVS_CL_GLOBAL_Z, VIVS_CL_GLOBAL_Z_SIZE(0x0) | VIVS_CL_GLOBAL_Z_OFFSET(0x0));
    etna_set_state(stream, VIVS_CL_WORKGROUP_X, VIVS_CL_WORKGROUP_X_SIZE(0x0) | VIVS_CL_WORKGROUP_X_COUNT(0x0));
    etna_set_state(stream, VIVS_CL_WORKGROUP_Y, VIVS_CL_WORKGROUP_Y_SIZE(0x3ff) | VIVS_CL_WORKGROUP_Y_COUNT(0xffff));
    etna_set_state(stream, VIVS_CL_WORKGROUP_Z, VIVS_CL_WORKGROUP_Z_SIZE(0x3ff) | VIVS_CL_WORKGROUP_Z_COUNT(0xffff));
    etna_set_state(stream, VIVS_CL_THREAD_ALLOCATION, 0x1);
    etna_set_state(stream, VIVS_CL_KICKER, 0xbadabeeb);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_TEXTURE | VIVS_GL_FLUSH_CACHE_SHADER_L1);
    etna_set_state(stream, VIVS_GL_SEMAPHORE_TOKEN, VIVS_GL_SEMAPHORE_TOKEN_FROM(SYNC_RECIPIENT_FE) | VIVS_GL_SEMAPHORE_TOKEN_TO(SYNC_RECIPIENT_PE));
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_DEPTH | VIVS_GL_FLUSH_CACHE_COLOR);
}

int main(int argc, char *argv[])
{
    const size_t out_size = 65536;

    struct etna_device *dev;
    struct etna_gpu *gpu;
    struct etna_pipe *pipe;
    struct etna_bo *bmp;
    struct etna_cmd_stream *stream;

    drmVersionPtr version;
    int fd, ret = 0;

        if (argc < 2) {
            printf("Usage: %s /dev/dri/...\n", argv[0]);
            return 1;
        }

    fd = open(argv[1], O_RDWR);
    if (fd < 0)
        return 1;

    version = drmGetVersion(fd);
    if (version) {
        printf("Version: %d.%d.%d\n", version->version_major,
               version->version_minor, version->version_patchlevel);
        printf("  Name: %s\n", version->name);
        printf("  Date: %s\n", version->date);
        printf("  Description: %s\n", version->desc);
        drmFreeVersion(version);
    }

    dev = etna_device_new(fd);
    if (!dev) {
        ret = 2;
        goto out;
    }

    /* TODO: we assume that core 1 is a 3D+CL capable one */
    gpu = etna_gpu_new(dev, 1);
    if (!gpu) {
        ret = 3;
        goto out_device;
    }

    pipe = etna_pipe_new(gpu, ETNA_PIPE_3D);
    if (!pipe) {
        ret = 4;
        goto out_gpu;
    }

    bmp = etna_bo_new(dev, out_size, DRM_ETNA_GEM_CACHE_UNCACHED);
    if (!bmp) {
        ret = 5;
        goto out_pipe;
    }
    memset(etna_bo_map(bmp), 0, out_size);

    stream = etna_cmd_stream_new(pipe, 0x3000, NULL, NULL);
    if (!stream) {
        ret = 6;
        goto out_bo;
    }

    /* generate command sequence */
    gen_cmd_stream(stream, bmp);

    etna_cmd_stream_finish(stream);

    const unsigned char *data = etna_bo_map(bmp);
    for(int i=0; i<0x100; ++i) {
        printf("%02x ", data[i]);
    }
    printf("\n");
    printf("%s\n", data);

    etna_cmd_stream_del(stream);

out_bo:
    etna_bo_del(bmp);

out_pipe:
    etna_pipe_del(pipe);

out_gpu:
    etna_gpu_del(gpu);

out_device:
    etna_device_del(dev);

out:
    close(fd);

    return ret;
}
