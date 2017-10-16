/*
 * Copyright (C) 2016 Etnaviv Project.
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
/* Basic "hello world" test */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "drm_setup.h"
#include "cmdstream.h"

#include "hw/state.xml.h"
#include "hw/state_3d.xml.h"
#include "hw/common.xml.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* GPU code to write "hello world" */
uint32_t hello_code[] = {
/*   0: */ 0x00801009, 0x00000000, 0x00000000, 0x20000008,  /* mov	t0.x___, void, void, u0.xxxx */
/*   1: */ 0x01001009, 0x00000000, 0x00000000, 0x00000008,  /* mov	t0._y__, void, void, t0.xxxx */
/*   2: */ 0x00800033, 0x15400c00, 0x80aa0040, 0x7400048a,  /* store.s8	mem.x___, t0.yyyy, u0.yyyy, ? */
/*   3: */ 0x00800033, 0x00000c00, 0x900000c0, 0x7400065f,  /* store.s8	mem.x___, t0.xxxx, ?, ? */
/*   4: */ 0x00800033, 0x00000c00, 0x90000140, 0x740006cf,  /* store.s8	mem.x___, t0.xxxx, ?, ? */
/*   5: */ 0x00800033, 0x00000c00, 0x900001c0, 0x740006cf,  /* store.s8	mem.x___, t0.xxxx, ?, ? */
/*   6: */ 0x00800033, 0x00000c00, 0x90000240, 0x740006ff,  /* store.s8	mem.x___, t0.xxxx, ?, ? */
/*   7: */ 0x00800033, 0x00000c00, 0x900002c0, 0x740002cf,  /* store.s8	mem.x___, t0.xxxx, ?, ? */
/*   8: */ 0x00800033, 0x00000c00, 0x90000340, 0x7400020f,  /* store.s8	mem.x___, t0.xxxx, ?, ? */
/*   9: */ 0x00800033, 0x00000c00, 0x900003c0, 0x7400057f,  /* store.s8	mem.x___, t0.xxxx, ?, ? */
/*  10: */ 0x00800033, 0x00000c00, 0x90000440, 0x740006ff,  /* store.s8	mem.x___, t0.xxxx, ?, ? */
/*  11: */ 0x00800033, 0x00000c00, 0x900004c0, 0x7400072f,  /* store.s8	mem.x___, t0.xxxx, ?, ? */
/*  12: */ 0x00800033, 0x00000c00, 0x90000540, 0x740006cf,  /* store.s8	mem.x___, t0.xxxx, ?, ? */
/*  13: */ 0x00800033, 0x00000c00, 0x900005c0, 0x7400064f,  /* store.s8	mem.x___, t0.xxxx, ?, ? */
/*  14: */ 0x00800033, 0x00000c00, 0x90000640, 0x7400021f,  /* store.s8	mem.x___, t0.xxxx, ?, ? */
/*  15: */ 0x00800033, 0x00000c00, 0x900006c0, 0x7400000f,  /* store.s8	mem.x___, t0.xxxx, ?, ? */
/*  16: */ 0x00000000, 0x00000000, 0x00000000, 0x00000000,  /* nop	void, void, void, void */
};

static void gen_cmd_stream(struct etna_cmd_stream *stream, struct etna_bo *code, struct etna_bo *bmp)
{
    etna_set_state(stream, VIVS_PA_SYSTEM_MODE, VIVS_PA_SYSTEM_MODE_PROVOKING_VERTEX_LAST | VIVS_PA_SYSTEM_MODE_HALF_PIXEL_CENTER);
    etna_set_state(stream, VIVS_GL_API_MODE, VIVS_GL_API_MODE_OPENCL);
    etna_set_state(stream, VIVS_VS_UNIFORM_CACHE, 0x1000);

    etna_set_state(stream, VIVS_PS_INPUT_COUNT, VIVS_PS_INPUT_COUNT_COUNT(1) | VIVS_PS_INPUT_COUNT_UNK8(31));
    etna_set_state(stream, VIVS_PS_TEMP_REGISTER_CONTROL, VIVS_PS_TEMP_REGISTER_CONTROL_NUM_TEMPS(4));
    etna_set_state(stream, VIVS_VS_OUTPUT_COUNT, 0);
    etna_set_state(stream, VIVS_CL_UNK00924, 0x0);
    etna_set_state(stream, VIVS_VS_UNIFORM_CACHE, 0x1011);
    etna_set_state_from_bo(stream, VIVS_SH_UNIFORMS(0), bmp, ETNA_RELOC_WRITE);
    etna_stall(stream, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
    etna_set_state(stream, VIVS_RA_CONTROL, VIVS_RA_CONTROL_UNK0);
    etna_set_state(stream, VIVS_PS_OUTPUT_REG, 0x0);
    etna_set_state(stream, VIVS_PS_TEMP_REGISTER_CONTROL, VIVS_PS_TEMP_REGISTER_CONTROL_NUM_TEMPS(4));
    etna_set_state(stream, VIVS_GL_VARYING_NUM_COMPONENTS, VIVS_GL_VARYING_NUM_COMPONENTS_VAR0(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR1(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR2(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR3(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR4(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR5(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR6(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR7(0x0));
    etna_set_state(stream, VIVS_GL_UNK03834, 0x0);
    etna_set_state(stream, VIVS_VS_UNIFORM_CACHE, 0x1011);
    etna_set_state(stream, VIVS_PS_UNIFORM_BASE, 0x0);
    etna_set_state(stream, VIVS_SH_UNIFORMS(1), 0x0);
    etna_set_state(stream, VIVS_VS_ICACHE_CONTROL, 0x21);
    etna_set_state(stream, VIVS_PS_RANGE, VIVS_PS_RANGE_LOW(0x0) | VIVS_PS_RANGE_HIGH(0xf));
    etna_set_state_from_bo(stream, VIVS_PS_INST_ADDR, code, ETNA_RELOC_READ);
    etna_set_state(stream, VIVS_PS_INPUT_COUNT, VIVS_PS_INPUT_COUNT_COUNT(1) | VIVS_PS_INPUT_COUNT_UNK8(31));
    etna_set_state(stream, VIVS_PS_TEMP_REGISTER_CONTROL, VIVS_PS_TEMP_REGISTER_CONTROL_NUM_TEMPS(4));
    etna_set_state(stream, VIVS_PS_CONTROL, 0);
    etna_set_state(stream, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT_UNK0(0x0) | VIVS_PA_ATTRIBUTE_ELEMENT_COUNT_COUNT(0x0));
    etna_set_state(stream, VIVS_GL_VARYING_TOTAL_COMPONENTS, VIVS_GL_VARYING_TOTAL_COMPONENTS_NUM(0x0));
    etna_set_state(stream, VIVS_PS_CONTROL_EXT, 0x0);
    etna_set_state(stream, VIVS_VS_LOAD_BALANCING, VIVS_VS_LOAD_BALANCING_A(0x0) | VIVS_VS_LOAD_BALANCING_B(0x0) | VIVS_VS_LOAD_BALANCING_C(0x3f) | VIVS_VS_LOAD_BALANCING_D(0xf));
    etna_set_state(stream, VIVS_VS_OUTPUT_COUNT, 1);
    etna_set_state(stream, VIVS_CL_CONFIG, VIVS_CL_CONFIG_DIMENSIONS(0x1) | VIVS_CL_CONFIG_TRAVERSE_ORDER(0x0) | VIVS_CL_CONFIG_SWATH_SIZE_X(0x0) | VIVS_CL_CONFIG_SWATH_SIZE_Y(0x0) | VIVS_CL_CONFIG_SWATH_SIZE_Z(0x0) | VIVS_CL_CONFIG_VALUE_ORDER(0x3));
    etna_set_state(stream, VIVS_CL_GLOBAL_X, VIVS_CL_GLOBAL_X_SIZE(0x1) | VIVS_CL_GLOBAL_X_OFFSET(0x0));
    etna_set_state(stream, VIVS_CL_GLOBAL_Y, VIVS_CL_GLOBAL_Y_SIZE(0x0) | VIVS_CL_GLOBAL_Y_OFFSET(0x0));
    etna_set_state(stream, VIVS_CL_GLOBAL_Z, VIVS_CL_GLOBAL_Z_SIZE(0x0) | VIVS_CL_GLOBAL_Z_OFFSET(0x0));
    etna_set_state(stream, VIVS_CL_WORKGROUP_X, VIVS_CL_WORKGROUP_X_SIZE(0x0) | VIVS_CL_WORKGROUP_X_COUNT(0x0));
    etna_set_state(stream, VIVS_CL_WORKGROUP_Y, VIVS_CL_WORKGROUP_Y_SIZE(0x3ff) | VIVS_CL_WORKGROUP_Y_COUNT(0xffff));
    etna_set_state(stream, VIVS_CL_WORKGROUP_Z, VIVS_CL_WORKGROUP_Z_SIZE(0x3ff) | VIVS_CL_WORKGROUP_Z_COUNT(0xffff));
    etna_set_state(stream, VIVS_CL_THREAD_ALLOCATION, 0x1);
    etna_set_state(stream, VIVS_CL_UNK00940, 0x0);
    etna_set_state(stream, VIVS_CL_UNK00944, 0xffffffff);
    etna_set_state(stream, VIVS_CL_UNK00948, 0xffffffff);
    etna_set_state(stream, VIVS_CL_UNK0094C, 0x0);
    etna_set_state(stream, VIVS_CL_UNK00950, 0x3ff);
    etna_set_state(stream, VIVS_CL_UNK00954, 0x3ff);
    etna_set_state(stream, VIVS_CL_KICKER, 0xbadabeeb);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_TEXTURE | VIVS_GL_FLUSH_CACHE_SHADER_L1);
    etna_stall(stream, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_DEPTH | VIVS_GL_FLUSH_CACHE_COLOR);
}

int main(int argc, char *argv[])
{
    struct drm_test_info *info;
    struct etna_bo *bmp, *code;
    static const size_t out_size = 65536;
    static const size_t code_size = 4096;
    if ((info = drm_test_setup(argc, argv)) == NULL) {
        return 1;
    }

    code = etna_bo_new(info->dev, code_size, DRM_ETNA_GEM_CACHE_UNCACHED);
    if (!code) {
        fprintf(stderr, "Unable to allocate buffer\n");
        goto out;
    }
    memcpy(etna_bo_map(code), hello_code, sizeof(hello_code));

    bmp = etna_bo_new(info->dev, out_size, DRM_ETNA_GEM_CACHE_UNCACHED);
    if (!bmp) {
        fprintf(stderr, "Unable to allocate buffer\n");
        goto out;
    }
    memset(etna_bo_map(bmp), 0, out_size);

    /* generate command sequence */
    gen_cmd_stream(info->stream, code, bmp);

    etna_cmd_stream_finish(info->stream);

    const unsigned char *data = etna_bo_map(bmp);
    for(int i=0; i<0x100; ++i) {
        printf("%02x ", data[i]);
    }
    printf("\n");
    printf("%s\n", data);

    drm_test_teardown(info);
    return 0;
out:
    drm_test_teardown(info);
    return 1;
}
