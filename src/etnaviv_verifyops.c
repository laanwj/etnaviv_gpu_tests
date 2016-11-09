/*
 * Copyright (C) 2016 Etnaviv Project.
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
/* Verify 2-input operation */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <time.h>

#include "drm_setup.h"
#include "cmdstream.h"
#include "memutil.h"

#include "state.xml.h"
#include "state_3d.xml.h"
#include "common.xml.h"

#define GPU_CODE(x) {x, ARRAY_SIZE(x)}
struct gpu_code {
    const uint32_t *code;
    unsigned size;
};

enum compare_type {
    CT_INT32,
    CT_FLOAT32
};

struct op_test {
    const char *op_name;
    size_t unit_size;
    enum compare_type compare_type;
    void (*generate_values_h)(size_t seed, void *a, size_t width);
    // Leave NULL for unary ops
    void (*generate_values_v)(size_t seed, void *b, size_t height);
    void (*compute_cpu)(void *out, const void *a, const void *b, size_t width, size_t height);
    struct gpu_code gpu_code;
};

struct gpu_code prelude = GPU_CODE(((uint32_t[]){
    0x0082100c, 0x15600800, 0x800100c0, 0x0000000a,  /* 0x4c.u32      t2.x___, t0.yyyy, u1.xxxx, t0.xxxx */
    0x01021019, 0x00200800, 0x80010000, 0x203fc008,  /* lshift.u32    t2._y__, t0.xxxx, void, u0.wwww */
    0x01011032, 0x15600800, 0x80aa0150, 0x00000000,  /* load.u32      t1._y__, u0.yyyy, t2.yyyy, void */
    0x01021009, 0x00000000, 0x00000000, 0x00154018,  /* mov   t2._y__, void, void, t1.yyyy            */
    0x00801019, 0x15600800, 0x80010000, 0x203fc008,  /* lshift.u32    t0.x___, t0.yyyy, void, u0.wwww */
    0x00811032, 0x2aa00800, 0x80000050, 0x00000000,  /* load.u32      t1.x___, u0.zzzz, t0.xxxx, void */
}));

struct gpu_code postlude = GPU_CODE(((uint32_t[]){
    0x01001019, 0x00202800, 0x80010000, 0x203fc008,  /* lshift.u32    t0._y__, t2.xxxx, void, u0.wwww */
    0x00800033, 0x00200800, 0x80aa0050, 0x00000008,  /* store.u32     mem.x___, u0.xxxx, t0.yyyy, t0.xxxx */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,  /* nop   void, void, void, void                  */
}));

#define MAX_INST 1024
static void gen_cmd_stream(struct etna_cmd_stream *stream, struct gpu_code *gpu_code, struct etna_bo *out, struct etna_bo *in0, struct etna_bo *in1)
{
    unsigned num_inst;
    uint32_t code[MAX_INST*4];
    unsigned code_ptr = 0;

    for (unsigned i=0; i<prelude.size; ++i)
        code[code_ptr++] = prelude.code[i];
    for (unsigned i=0; i<gpu_code->size; ++i)
        code[code_ptr++] = gpu_code->code[i];
    for (unsigned i=0; i<postlude.size; ++i)
        code[code_ptr++] = postlude.code[i];
    assert((code_ptr & 3)==0);
    num_inst = code_ptr / 4; // number of instructions including final nop

    etna_set_state(stream, VIVS_PA_SYSTEM_MODE, VIVS_PA_SYSTEM_MODE_UNK0 | VIVS_PA_SYSTEM_MODE_UNK4);
    etna_set_state(stream, VIVS_GL_API_MODE, VIVS_GL_API_MODE_OPENCL);

    etna_set_state(stream, VIVS_VS_INPUT_COUNT, VIVS_VS_INPUT_COUNT_COUNT(1) | VIVS_VS_INPUT_COUNT_UNK8(31));
    etna_set_state(stream, VIVS_VS_INPUT(0), VIVS_VS_INPUT_I0(0) | VIVS_VS_INPUT_I1(1) | VIVS_VS_INPUT_I2(2) | VIVS_VS_INPUT_I3(3));
    etna_set_state(stream, VIVS_VS_TEMP_REGISTER_CONTROL, VIVS_VS_TEMP_REGISTER_CONTROL_NUM_TEMPS(10));
    etna_set_state(stream, VIVS_VS_OUTPUT_COUNT, 0);
    etna_set_state(stream, VIVS_CL_UNK00924, 0x0);

    etna_set_state_from_bo(stream, VIVS_VS_UNIFORMS(0), out);
    etna_set_state_from_bo(stream, VIVS_VS_UNIFORMS(1), in0);
    etna_set_state_from_bo(stream, VIVS_VS_UNIFORMS(2), in1);
    etna_set_state(stream, VIVS_VS_UNIFORMS(3), 0x2);
    etna_set_state(stream, VIVS_VS_UNIFORMS(4), 0x10);
    etna_set_state(stream, VIVS_VS_UNIFORMS(5), 0x10);
    etna_set_state(stream, VIVS_VS_UNIFORMS(6), 0x0);
    etna_set_state(stream, VIVS_VS_UNIFORMS(7), 0x0);

    etna_set_state(stream, VIVS_GL_SEMAPHORE_TOKEN, VIVS_GL_SEMAPHORE_TOKEN_FROM(SYNC_RECIPIENT_FE) | VIVS_GL_SEMAPHORE_TOKEN_TO(SYNC_RECIPIENT_PE));
    etna_set_state(stream, VIVS_VS_INPUT_COUNT, VIVS_VS_INPUT_COUNT_COUNT(1) | VIVS_VS_INPUT_COUNT_UNK8(1));
    etna_set_state(stream, VIVS_VS_TEMP_REGISTER_CONTROL, VIVS_VS_TEMP_REGISTER_CONTROL_NUM_TEMPS(10));
    etna_set_state(stream, VIVS_VS_OUTPUT(0), VIVS_VS_OUTPUT_O0(0) | VIVS_VS_OUTPUT_O1(0) | VIVS_VS_OUTPUT_O2(0) | VIVS_VS_OUTPUT_O3(0));
    etna_set_state(stream, VIVS_GL_VARYING_NUM_COMPONENTS, VIVS_GL_VARYING_NUM_COMPONENTS_VAR0(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR1(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR2(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR3(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR4(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR5(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR6(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR7(0x0));
    etna_set_state(stream, VIVS_GL_UNK03834, 0x0);
    etna_set_state(stream, VIVS_VS_NEW_UNK00860, 0x0);
    etna_set_state(stream, VIVS_VS_RANGE, VIVS_VS_RANGE_LOW(0x0) | VIVS_VS_RANGE_HIGH(num_inst - 2));

    for (unsigned i=0; i<code_ptr; ++i)
        etna_set_state(stream, VIVS_SH_INST_MEM(i), code[i]);

    etna_set_state(stream, VIVS_PS_INPUT_COUNT, VIVS_PS_INPUT_COUNT_COUNT(1) | VIVS_PS_INPUT_COUNT_UNK8(31));
    etna_set_state(stream, VIVS_PS_TEMP_REGISTER_CONTROL, VIVS_PS_TEMP_REGISTER_CONTROL_NUM_TEMPS(10));
    etna_set_state(stream, VIVS_PS_CONTROL, 0);
    etna_set_state(stream, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT_UNK0(0x0) | VIVS_PA_ATTRIBUTE_ELEMENT_COUNT_COUNT(0x0));
    etna_set_state(stream, VIVS_GL_VARYING_TOTAL_COMPONENTS, VIVS_GL_VARYING_TOTAL_COMPONENTS_NUM(0x0));
    etna_set_state(stream, VIVS_PS_UNK01030, 0x0);
    etna_set_state(stream, VIVS_VS_LOAD_BALANCING, VIVS_VS_LOAD_BALANCING_A(0x42) | VIVS_VS_LOAD_BALANCING_B(0x5) | VIVS_VS_LOAD_BALANCING_C(0x3f) | VIVS_VS_LOAD_BALANCING_D(0xf));
    etna_set_state(stream, VIVS_VS_OUTPUT_COUNT, 1);
    etna_set_state(stream, VIVS_CL_CONFIG, VIVS_CL_CONFIG_DIMENSIONS(0x2) | VIVS_CL_CONFIG_TRAVERSE_ORDER(0x0) | VIVS_CL_CONFIG_SWATH_SIZE_X(0x0) | VIVS_CL_CONFIG_SWATH_SIZE_Y(0x0) | VIVS_CL_CONFIG_SWATH_SIZE_Z(0x0) | VIVS_CL_CONFIG_VALUE_ORDER(0x3));
    etna_set_state(stream, VIVS_CL_GLOBAL_X, VIVS_CL_GLOBAL_X_SIZE(0x10) | VIVS_CL_GLOBAL_X_OFFSET(0x0));
    etna_set_state(stream, VIVS_CL_GLOBAL_Y, VIVS_CL_GLOBAL_Y_SIZE(0x10) | VIVS_CL_GLOBAL_Y_OFFSET(0x0));
    etna_set_state(stream, VIVS_CL_GLOBAL_Z, VIVS_CL_GLOBAL_Z_SIZE(0x0) | VIVS_CL_GLOBAL_Z_OFFSET(0x0));
    etna_set_state(stream, VIVS_CL_WORKGROUP_X, VIVS_CL_WORKGROUP_X_SIZE(0x7) | VIVS_CL_WORKGROUP_X_COUNT(0x1));
    etna_set_state(stream, VIVS_CL_WORKGROUP_Y, VIVS_CL_WORKGROUP_Y_SIZE(0x7) | VIVS_CL_WORKGROUP_Y_COUNT(0x1));
    etna_set_state(stream, VIVS_CL_WORKGROUP_Z, VIVS_CL_WORKGROUP_Z_SIZE(0x3ff) | VIVS_CL_WORKGROUP_Z_COUNT(0xffff));
    etna_set_state(stream, VIVS_CL_THREAD_ALLOCATION, 0x4);
    // Kick off program
    etna_set_state(stream, VIVS_CL_KICKER, 0xbadabeeb);

    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_TEXTURE | VIVS_GL_FLUSH_CACHE_SHADER_L1);
    etna_set_state(stream, VIVS_GL_SEMAPHORE_TOKEN, VIVS_GL_SEMAPHORE_TOKEN_FROM(SYNC_RECIPIENT_FE) | VIVS_GL_SEMAPHORE_TOKEN_TO(SYNC_RECIPIENT_PE));
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_DEPTH | VIVS_GL_FLUSH_CACHE_COLOR);
}

void i32_generate_values_h(size_t seed, void *a, size_t width)
{
    uint32_t base = seed * width;
    for (size_t x=0; x<width; ++x) {
        ((uint32_t*)a)[x] = base + x;
    }
}

void i32_generate_values_v(size_t seed, void *b, size_t height)
{
    uint32_t base = seed * height;
    for (size_t y=0; y<height; ++y) {
        ((uint32_t*)b)[y] = base + y;
    }
}

static void addu32_compute_cpu(void *out_, const void *a_, const void *b_, size_t width, size_t height)
{
    uint32_t *out = (uint32_t*)out_;
    uint32_t *a = (uint32_t*)a_;
    uint32_t *b = (uint32_t*)b_;

    for(size_t y=0; y<height; ++y) {
        for(size_t x=0; x<width; ++x) {
            out[y*width+x] = a[x] + b[y];
        }
    }
}

static void mulu32_compute_cpu(void *out_, const void *a_, const void *b_, size_t width, size_t height)
{
    uint32_t *out = (uint32_t*)out_;
    uint32_t *a = (uint32_t*)a_;
    uint32_t *b = (uint32_t*)b_;

    for(size_t y=0; y<height; ++y) {
        for(size_t x=0; x<width; ++x) {
            out[y*width+x] = a[x] * b[y];
        }
    }
}

static void addf32_compute_cpu(void *out_, const void *a_, const void *b_, size_t width, size_t height)
{
    float *out = (float*)out_;
    float *a = (float*)a_;
    float *b = (float*)b_;

    for(size_t y=0; y<height; ++y) {
        for(size_t x=0; x<width; ++x) {
            out[y*width+x] = a[x] + b[y];
        }
    }
}

/* Tests GPU code must take from t2 and t1, and output to t0 */
struct op_test op_tests[] = {
    {"add.u32", 4, CT_INT32, i32_generate_values_h, i32_generate_values_v, addu32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x00801001, 0x15602800, 0x80000000, 0x00000018, /* add.u32       t0.x___, t2.yyyy, void, t1.xxxx */
        }))
    },
    // add.u16 does nothing
    // 0x00801001, 0x15402800, 0xc0000000, 0x00000018, /* add.u16       t0.x___, t2.yyyy, void, t1.xxxx */
#if 0
    {"add.f32", 4, CT_FLOAT32, i32_generate_values_h, i32_generate_values_v, addf32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x00801001, 0x15402800, 0x00000000, 0x00000018, /* add.f32       t0.x___, t2.yyyy, void, t1.xxxx */
        }))
    },
#endif
    {"imullo0.u32", 4, CT_INT32, i32_generate_values_h, i32_generate_values_v, mulu32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0080103c, 0x15602800, 0x800000c0, 0x00000000, /* imullo0.u32   t0.x___, t2.yyyy, t1.xxxx, void */
        }))
    },
};

int perform_test(struct drm_test_info *info, struct op_test *cur_test)
{
    int retval = -1;
    const size_t width = 16;
    const size_t height = 16;
    size_t seedx, seedy;
    struct etna_bo *bo_out=0, *bo_in0=0, *bo_in1=0;
    unsigned int errors = 0;

    size_t out_size = width * height * cur_test->unit_size;
    size_t in0_size = width * cur_test->unit_size;
    size_t in1_size = height * cur_test->unit_size;

    void *out_cpu = malloc(out_size);
    void *a_cpu = malloc(in0_size);
    void *b_cpu = malloc(in1_size);

    printf("%s: ", cur_test->op_name);
    fflush(stdout);

    bo_out = etna_bo_new(info->dev, out_size, DRM_ETNA_GEM_CACHE_UNCACHED);
    bo_in0 = etna_bo_new(info->dev, in0_size, DRM_ETNA_GEM_CACHE_UNCACHED);
    bo_in1 = etna_bo_new(info->dev, in1_size, DRM_ETNA_GEM_CACHE_UNCACHED);
    if (!bo_in0 || !bo_in1 || !bo_out) {
        fprintf(stderr, "Unable to allocate buffer\n");
        goto out;
    }
    for (int num_tries=0; num_tries<100 && !errors; ++num_tries) {
        seedx = rand();
        seedy = rand();
        cur_test->generate_values_h(seedx, a_cpu, width);
        cur_test->generate_values_v(seedy, b_cpu, height);
        cur_test->compute_cpu(out_cpu, a_cpu, b_cpu, width, height);

        memset(etna_bo_map(bo_out), 0, out_size);
        memcpy(etna_bo_map(bo_in0), a_cpu, in0_size);
        memcpy(etna_bo_map(bo_in1), b_cpu, in1_size);

        /* generate command sequence */
        gen_cmd_stream(info->stream, &cur_test->gpu_code, bo_out, bo_in0, bo_in1);

        etna_cmd_stream_finish(info->stream);

        const uint32_t *out_gpu = etna_bo_map(bo_out);
        if (cur_test->unit_size == 4 && cur_test->compare_type == CT_INT32) {
            for(size_t y=0; y<height; ++y) {
                for(size_t x=0; x<height; ++x) {
                    uint32_t expected = ((uint32_t*)out_cpu)[y*width+x];
                    uint32_t found = ((uint32_t*)out_gpu)[y*width+x];
                    if (expected != found) {
                        if (errors < 10)
                            printf("Mismatch %s(%08x,%08x) -> %08x, expected %08x\n", cur_test->op_name, ((uint32_t*)a_cpu)[x], ((uint32_t*)b_cpu)[y], found, expected);
                        errors += 1;
                    }
                }
            }
        } else {
            errors = 1;
            printf("No comparison implemented for unit_size %d compare_type %d\n", (int)cur_test->unit_size, cur_test->compare_type);
        }
    }
    if (errors == 0) {
        printf("PASS\n");
        retval = 0;
    } else {
        printf("FAIL (seedx %u seedy %u)\n", (unsigned)seedx, (unsigned)seedy);
        retval = 1;
    }

out:
    etna_bo_del(bo_out);
    etna_bo_del(bo_in0);
    etna_bo_del(bo_in1);

    free(out_cpu);
    free(a_cpu);
    free(b_cpu);
    return retval;
}

int main(int argc, char *argv[])
{
    srand(time(NULL));
    struct drm_test_info *info;
    if ((info = drm_test_setup(argc, argv)) == NULL) {
        return 1;
    }
    for (unsigned t=0; t<ARRAY_SIZE(op_tests); ++t)
    {
        perform_test(info, &op_tests[t]);
    }

    drm_test_teardown(info);
    return 0;
    drm_test_teardown(info);
    return 1;
}

