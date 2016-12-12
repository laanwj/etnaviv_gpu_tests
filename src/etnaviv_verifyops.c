/*
 * Copyright (C) 2016 Etnaviv Project.
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
/* Verify ALU operations */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <time.h>

#include "drm_setup.h"
#include "cmdstream.h"
#include "memutil.h"
#include "float_helpers.h"

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
    CT_INT32_BCAST,
    CT_FLOAT32,
    CT_FLOAT32_BCAST
};

enum hardware_type {
    HWT_GC2000 = 1,
    HWT_GC3000 = 2,
    HWT_ALL = 3,
};

struct op_test {
    const char *op_name;
    enum hardware_type hardware_type;
    enum compare_type compare_type;
    void (*generate_values_h)(size_t seed, void *a, size_t width);
    // Leave NULL for unary ops
    void (*generate_values_v)(size_t seed, void *b, size_t height);
    void (*compute_cpu)(void *out, const void *a, const void *b, size_t width, size_t height);
    struct gpu_code gpu_code;
    uint32_t auxin[4];
};

struct gpu_code prelude = GPU_CODE(((uint32_t[]){
    0x00821019, 0x00200800, 0x80010000, 0x203fc008,  /* lshift.u32  t2.x___, t0.xxxx, void, u0.wwww */
    0x07821032, 0x15600800, 0x80000150, 0x00000000,  /* load.u32    t2, u0.yyyy, t2.xxxx, void */
    0x00831019, 0x15600800, 0x80010000, 0x203fc008,  /* lshift.u32  t3.x___, t0.yyyy, void, u0.wwww */
    0x07831032, 0x2aa00800, 0x800001d0, 0x00000000,  /* load.u32    t3, u0.zzzz, t3.xxxx, void */
    0x07841009, 0x00000000, 0x00000000, 0x20390028,  /* mov         t4, void, void, u2 */
}));

struct gpu_code postlude = GPU_CODE(((uint32_t[]){
    0x0080100c, 0x15600800, 0x800100c0, 0x0000000a,  /* imadlo0.u32 t0.x___, t0.yyyy, u1.xxxx, t0.xxxx */
    0x00801019, 0x00200800, 0x80010000, 0x203fc008,  /* lshift.u32  t0.x___, t0.xxxx, void, u0.wwww */
    0x07800033, 0x00200800, 0x80000050, 0x00390048,  /* store.u32   mem.xyzw, u0.xxxx, t0.xxxx, t4 */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,  /* nop void, void, void, void */
}));

static const char *COMPS = "xyzw";

static void gen_cmd_stream(enum hardware_type hwt, struct etna_cmd_stream *stream, struct gpu_code *gpu_code, struct etna_bo *out, struct etna_bo *in0, struct etna_bo *in1, uint32_t *auxin)
{
    unsigned uniform_base = 0;
    unsigned inst_range_max = gpu_code->size / 4 - 2;

    etna_set_state(stream, VIVS_PA_SYSTEM_MODE, VIVS_PA_SYSTEM_MODE_UNK0 | VIVS_PA_SYSTEM_MODE_UNK4);
    etna_set_state(stream, VIVS_GL_API_MODE, VIVS_GL_API_MODE_OPENCL);

    if (hwt == HWT_GC2000) {
        /* Need to write *something* to VS input registers before writing shader uniforms and code. Otherwise
         * the whole thing will hang when running this first after boot.
         */
        etna_set_state(stream, VIVS_VS_INPUT_COUNT, VIVS_VS_INPUT_COUNT_COUNT(1) | VIVS_VS_INPUT_COUNT_UNK8(31));
        etna_set_state(stream, VIVS_VS_INPUT(0), VIVS_VS_INPUT_I0(0) | VIVS_VS_INPUT_I1(1) | VIVS_VS_INPUT_I2(2) | VIVS_VS_INPUT_I3(3));
    }

    if (hwt == HWT_GC3000) {
        /* GC3000: unified uniforms, shader instructions in memory */
        uniform_base = VIVS_SH_UNIFORMS(0);
        etna_set_state(stream, VIVS_VS_ICACHE_CONTROL, 0x21);
        assert(gpu_code->bo);
        etna_set_state_from_bo(stream, VIVS_PS_INST_ADDR, gpu_code->bo, ETNA_RELOC_READ);

    } else if (hwt == HWT_GC2000) {
        /* GC2000: VS uniforms, shader instructions on-chip */
        uniform_base = VIVS_VS_UNIFORMS(0);
        for (unsigned i=0; i<gpu_code->size; ++i)
            etna_set_state(stream, VIVS_SH_INST_MEM(i), gpu_code->code[i]);
    }

    /* Set uniforms */
    etna_set_state_from_bo(stream, uniform_base + 0*4, out, ETNA_RELOC_WRITE); /* u0.x */
    etna_set_state_from_bo(stream, uniform_base + 1*4, in0, ETNA_RELOC_READ); /* u0.y */
    etna_set_state_from_bo(stream, uniform_base + 2*4, in1, ETNA_RELOC_READ); /* u0.z */
    etna_set_state(stream, uniform_base + 3*4, 0x4);  /* u0.w Left-shift */
    etna_set_state(stream, uniform_base + 4*4, 0x10); /* u1.x Row stride */
    etna_set_state(stream, uniform_base + 5*4, 0x0);  /* u1.y Unused */
    etna_set_state(stream, uniform_base + 6*4, 0x0);  /* u1.z Unused */
    etna_set_state(stream, uniform_base + 7*4, 0x0);  /* u1.w Unused */
    etna_set_state(stream, uniform_base + 8*4, 0xaaaaaaaa); /* u2.x Default output (if GPU program generates no output in t4) */
    etna_set_state(stream, uniform_base + 9*4, 0x55555555); /* u2.y */
    etna_set_state(stream, uniform_base + 10*4, 0xaaaaaaaa); /* u2.z */
    etna_set_state(stream, uniform_base + 11*4, 0x55555555); /* u2.w */
    etna_set_state(stream, uniform_base + 12*4, auxin[0]); /* u3.x Ancillary input for testing three-operand instructions */
    etna_set_state(stream, uniform_base + 13*4, auxin[1]); /* u3.y */
    etna_set_state(stream, uniform_base + 14*4, auxin[2]); /* u3.z */
    etna_set_state(stream, uniform_base + 15*4, auxin[3]); /* u3.w */

    etna_set_state(stream, VIVS_VS_INPUT_COUNT, VIVS_VS_INPUT_COUNT_COUNT(1) | VIVS_VS_INPUT_COUNT_UNK8(1));
    etna_set_state(stream, VIVS_VS_TEMP_REGISTER_CONTROL, VIVS_VS_TEMP_REGISTER_CONTROL_NUM_TEMPS(10));
    etna_set_state(stream, VIVS_VS_OUTPUT(0), VIVS_VS_OUTPUT_O0(0) | VIVS_VS_OUTPUT_O1(0) | VIVS_VS_OUTPUT_O2(0) | VIVS_VS_OUTPUT_O3(0));
    /* Unknown state set differently for GC2000 and GC3000 */
    if (hwt == HWT_GC3000) {
        etna_set_state(stream, VIVS_VS_NEW_UNK00860, 0x1011); /* PS/VS units? */
    } else if (hwt == HWT_GC2000) {
        etna_set_state(stream, VIVS_VS_NEW_UNK00860, 0x0);
    }
    etna_set_state(stream, VIVS_VS_RANGE, VIVS_VS_RANGE_LOW(0x0) | VIVS_VS_RANGE_HIGH(inst_range_max));
    etna_set_state(stream, VIVS_VS_LOAD_BALANCING, VIVS_VS_LOAD_BALANCING_A(0x42) | VIVS_VS_LOAD_BALANCING_B(0x5) | VIVS_VS_LOAD_BALANCING_C(0x3f) | VIVS_VS_LOAD_BALANCING_D(0xf));
    etna_set_state(stream, VIVS_VS_OUTPUT_COUNT, 1);

    etna_set_state(stream, VIVS_GL_VARYING_NUM_COMPONENTS, VIVS_GL_VARYING_NUM_COMPONENTS_VAR0(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR1(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR2(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR3(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR4(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR5(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR6(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR7(0x0));
    etna_set_state(stream, VIVS_GL_UNK03834, 0x0);
    etna_set_state(stream, VIVS_GL_VARYING_TOTAL_COMPONENTS, VIVS_GL_VARYING_TOTAL_COMPONENTS_NUM(0x0));

    etna_set_state(stream, VIVS_PS_INPUT_COUNT, VIVS_PS_INPUT_COUNT_COUNT(1) | VIVS_PS_INPUT_COUNT_UNK8(31));
    etna_set_state(stream, VIVS_PS_TEMP_REGISTER_CONTROL, VIVS_PS_TEMP_REGISTER_CONTROL_NUM_TEMPS(10));
    etna_set_state(stream, VIVS_PS_CONTROL, 0);
    etna_set_state(stream, VIVS_PS_CONTROL_EXT, 0x0);

    if (hwt == HWT_GC3000) {
        /* GC3000: Needs some PA state */
        etna_set_state(stream, VIVS_PA_SHADER_ATTRIBUTES(0), VIVS_PA_SHADER_ATTRIBUTES_UNK4(0x0) | VIVS_PA_SHADER_ATTRIBUTES_UNK8(0x2));
        etna_set_state(stream, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT_UNK0(0x0) | VIVS_PA_ATTRIBUTE_ELEMENT_COUNT_COUNT(0x1));

    } else if (hwt == HWT_GC2000) {

        /* GC2000: Disable PA */
        etna_set_state(stream, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT_UNK0(0x0) | VIVS_PA_ATTRIBUTE_ELEMENT_COUNT_COUNT(0x0));
    }

    etna_set_state(stream, VIVS_CL_UNK00924, 0x0);
    etna_set_state(stream, VIVS_CL_CONFIG, VIVS_CL_CONFIG_DIMENSIONS(0x2) | VIVS_CL_CONFIG_TRAVERSE_ORDER(0x0) | VIVS_CL_CONFIG_SWATH_SIZE_X(0x0) | VIVS_CL_CONFIG_SWATH_SIZE_Y(0x0) | VIVS_CL_CONFIG_SWATH_SIZE_Z(0x0) | VIVS_CL_CONFIG_VALUE_ORDER(0x3));
    etna_set_state(stream, VIVS_CL_GLOBAL_X, VIVS_CL_GLOBAL_X_SIZE(0x10) | VIVS_CL_GLOBAL_X_OFFSET(0x0));
    etna_set_state(stream, VIVS_CL_GLOBAL_Y, VIVS_CL_GLOBAL_Y_SIZE(0x10) | VIVS_CL_GLOBAL_Y_OFFSET(0x0));
    etna_set_state(stream, VIVS_CL_GLOBAL_Z, VIVS_CL_GLOBAL_Z_SIZE(0x0) | VIVS_CL_GLOBAL_Z_OFFSET(0x0));
    etna_set_state(stream, VIVS_CL_WORKGROUP_X, VIVS_CL_WORKGROUP_X_SIZE(0x7) | VIVS_CL_WORKGROUP_X_COUNT(0x1));
    etna_set_state(stream, VIVS_CL_WORKGROUP_Y, VIVS_CL_WORKGROUP_Y_SIZE(0x7) | VIVS_CL_WORKGROUP_Y_COUNT(0x1));
    etna_set_state(stream, VIVS_CL_WORKGROUP_Z, VIVS_CL_WORKGROUP_Z_SIZE(0x3ff) | VIVS_CL_WORKGROUP_Z_COUNT(0xffff));
    etna_set_state(stream, VIVS_CL_THREAD_ALLOCATION, 0x4);

    if (hwt == HWT_GC3000) {

        /* GC3000-only unknown state */
        etna_set_state(stream, VIVS_RA_CONTROL, VIVS_RA_CONTROL_UNK0);
        etna_set_state(stream, VIVS_PS_UNIFORM_BASE, 0x0);
        /* GC3000 uses the PS_RANGE instead of VS_RANGE for marking the CL shader instruction range */
        etna_set_state(stream, VIVS_PS_RANGE, VIVS_PS_RANGE_LOW(0x0) | VIVS_PS_RANGE_HIGH(inst_range_max));
        /* GC3000: Needs PS output register */
        etna_set_state(stream, VIVS_PS_OUTPUT_REG, 0x0);
        /* Load balancing set differently for GC3000 */
        etna_set_state(stream, VIVS_VS_LOAD_BALANCING, VIVS_VS_LOAD_BALANCING_A(0x0) | VIVS_VS_LOAD_BALANCING_B(0x0) | VIVS_VS_LOAD_BALANCING_C(0x3f) | VIVS_VS_LOAD_BALANCING_D(0xf));
        /* GC3000: Extra registers that seem to mirror CL_GLOBAL and CL_WORKGROUP */
        etna_set_state(stream, VIVS_CL_UNK00940, 0x1);
        etna_set_state(stream, VIVS_CL_UNK00944, 0x1);
        etna_set_state(stream, VIVS_CL_UNK00948, 0xffffffff);
        etna_set_state(stream, VIVS_CL_UNK0094C, 0x7);
        etna_set_state(stream, VIVS_CL_UNK00950, 0x7);
        etna_set_state(stream, VIVS_CL_UNK00954, 0x3ff);
    }

    /* Kick off program */
    etna_set_state(stream, VIVS_CL_KICKER, 0xbadabeeb);
    /* Flush caches so that we can see the output */
    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_TEXTURE | VIVS_GL_FLUSH_CACHE_SHADER_L1);
}

/** Generate horizontal and vertical testing table values */
void i32_generate_values_h(size_t seed, void *a, size_t width)
{
    uint32_t base = seed * width;
    for (size_t x=0; x<width; ++x) {
        ((uint32_t*)a)[x*4+0] = base + x;
        ((uint32_t*)a)[x*4+1] = 0x51515151; /* fill other vector elements with recognizable random pattern */
        ((uint32_t*)a)[x*4+2] = 0x15151515;
        ((uint32_t*)a)[x*4+3] = 0x36363636;
    }
}

void i32_generate_values_h4(size_t seed, void *a, size_t width)
{
    uint32_t base = seed * width;
    for (size_t x=0; x<width; ++x) {
        ((uint32_t*)a)[x*4+0] = base + x;
        ((uint32_t*)a)[x*4+1] = base + x + 0x010001;
        ((uint32_t*)a)[x*4+2] = base + x + 0x020002;
        ((uint32_t*)a)[x*4+3] = base + x + 0x030003;
    }
}

void i32_generate_values_v(size_t seed, void *b, size_t height)
{
    uint32_t base = seed * height;
    for (size_t y=0; y<height; ++y) {
        ((uint32_t*)b)[y*4+0] = base + y;
        ((uint32_t*)b)[y*4+1] = 0x82828282; /* fill other vector elements with recognizable random pattern */
        ((uint32_t*)b)[y*4+2] = 0x48484848;
        ((uint32_t*)b)[y*4+3] = 0x27272727;
    }
}

void i32_generate_values_v4(size_t seed, void *b, size_t height)
{
    uint32_t base = seed * height;
    for (size_t y=0; y<height; ++y) {
        ((uint32_t*)b)[y*4+0] = base + y;
        ((uint32_t*)b)[y*4+1] = base + y + 0x010001;
        ((uint32_t*)b)[y*4+2] = base + y + 0x020002;
        ((uint32_t*)b)[y*4+3] = base + y + 0x030003;
    }
}

/** CPU-side helper emulation functions */

/* Float to integer conversion emulation for GC2000. There is a significant
   difference from GC3000 here:
   - NaN is is converted to 0x80000000/0x7fffffff instead of 0x00000000
 */
static inline int32_t f2i_s32_gc2000(float f)
{
    if (isnan(f)) {
        uint32_t u = fui(f);
        if (u & 0x80000000) {
            return 0x80000000; /* "negative NaN" */
        } else {
            return 0x7fffffff; /* "postiive NaN" */
        }
    } else {
        return f;
    }
}
static inline uint32_t f2i_u32_gc2000(float f)
{
    if (isnan(f)) {
        uint32_t u = fui(f);
        if (u & 0x80000000) {
            return 0x00000000; /* "negative NaN" */
        } else {
            return 0xffffffff; /* "postiive NaN" */
        }
    } else {
        return f;
    }
}

/** Testing macros for generating CPU implementations */

/* shortcut for source value 0 */
#define A (a[x*4])
#define B (b[y*4])
/* source value (component) */
#define AI(i) (a[x*4+(i)])
#define BI(i) (b[y*4+(i)])
/* Scalar computations broadcasted to all channels */
#define CPU_COMPUTE_FUNC1_BCAST(_name, _type, _expr) \
    static void _name(_type *out, const _type *a, const _type *b, size_t width, size_t height) \
    { \
        for(size_t y=0; y<height; ++y) { \
            for(size_t x=0; x<width; ++x) { \
                out[(y*width+x)*4+0] = out[(y*width+x)*4+1] = out[(y*width+x)*4+2] = out[(y*width+x)*4+3] = (_expr); \
            } \
        } \
    }
/* Scalar computations broadcasted to all channels - conversion between types */
#define CPU_COMPUTE_FUNC1_CVT_BCAST(_name, _typeo, _typei, _expr) \
    static void _name(_typeo *out, const _typei *a, const _typei *b, size_t width, size_t height) \
    { \
        for(size_t y=0; y<height; ++y) { \
            for(size_t x=0; x<width; ++x) { \
                out[(y*width+x)*4+0] = out[(y*width+x)*4+1] = out[(y*width+x)*4+2] = out[(y*width+x)*4+3] = (_expr); \
            } \
        } \
    }
/* Scalar computation per channel (use index i) */
#define CPU_COMPUTE_FUNC1_MULTI(_name, _type, _expr) \
    static void _name(_type *out, const _type *a, const _type *b, size_t width, size_t height) \
    { \
        for(size_t y=0; y<height; ++y) { \
            for(size_t x=0; x<width; ++x) { \
                for(size_t i=0; i<4; ++i) { \
                    out[(y*width+x)*4+i] = (_expr); \
                } \
            } \
        } \
    }
/* Scalar conversion per channel (use index i) */
#define CPU_COMPUTE_FUNC1_CVT_MULTI(_name, _typeo, _typei, _expr) \
    static void _name(_typeo *out, const _typei *a, const _typei *b, size_t width, size_t height) \
    { \
        for(size_t y=0; y<height; ++y) { \
            for(size_t x=0; x<width; ++x) { \
                for(size_t i=0; i<4; ++i) { \
                    out[(y*width+x)*4+i] = (_expr); \
                } \
            } \
        } \
    }
/* Scalar computation on one channel only, rest will stay at padding pattern */
#define CPU_COMPUTE_FUNC1_PAD(_name, _type, _expr) \
    static void _name(_type *out, const _type *a, const _type *b, size_t width, size_t height) \
    { \
        for(size_t y=0; y<height; ++y) { \
            for(size_t x=0; x<width; ++x) { \
                out[(y*width+x)*4+0] = (_expr); \
                out[(y*width+x)*4+1] = 0x55555555; \
                out[(y*width+x)*4+2] = 0xaaaaaaaa; \
                out[(y*width+x)*4+3] = 0x55555555; \
            } \
        } \
    }
/* Independent expressions for channels */
#define CPU_COMPUTE_FUNC4(_name, _type, _expr0, _expr1, _expr2, _expr3) \
    static void _name(_type *out, const _type *a, const _type *b, size_t width, size_t height) \
    { \
        for(size_t y=0; y<height; ++y) { \
            for(size_t x=0; x<width; ++x) { \
                out[(y*width+x)*4+0] = (_expr0); \
                out[(y*width+x)*4+1] = (_expr1); \
                out[(y*width+x)*4+2] = (_expr2); \
                out[(y*width+x)*4+3] = (_expr3); \
            } \
        } \
    }
CPU_COMPUTE_FUNC4(nop_compute_cpu, uint32_t, 0xaaaaaaaa, 0x55555555, 0xaaaaaaaa, 0x55555555);
/* 1-wide u32 */
CPU_COMPUTE_FUNC1_PAD(addu32_single_compute_cpu, uint32_t, A + B);
CPU_COMPUTE_FUNC1_BCAST(addu32_compute_cpu, uint32_t, A + B);
CPU_COMPUTE_FUNC1_BCAST(mulu32_compute_cpu, uint32_t, A * B);
CPU_COMPUTE_FUNC1_BCAST(mulhu32_compute_cpu, uint32_t, ((uint64_t)A * (uint64_t)B)>>32);
CPU_COMPUTE_FUNC1_BCAST(madu32_compute_cpu, uint32_t, A * B + 0x12345678);
CPU_COMPUTE_FUNC1_BCAST(lshiftu32_compute_cpu, uint32_t, A << (B&31));
CPU_COMPUTE_FUNC1_BCAST(rshiftu32_compute_cpu, uint32_t, A >> (B&31));
CPU_COMPUTE_FUNC1_BCAST(rotateu32_compute_cpu, uint32_t, (A << (B&31)) | (A >> ((32-B)&31)));
CPU_COMPUTE_FUNC1_BCAST(oru32_compute_cpu, uint32_t, A | B);
CPU_COMPUTE_FUNC1_BCAST(andu32_compute_cpu, uint32_t, A & B);
CPU_COMPUTE_FUNC1_BCAST(xoru32_compute_cpu, uint32_t, A ^ B);
CPU_COMPUTE_FUNC1_BCAST(notu32_compute_cpu, uint32_t, ~A);
CPU_COMPUTE_FUNC1_BCAST(leadzerou32_compute_cpu, uint32_t, (A != 0) ? __builtin_clz(A) : 0x20);
CPU_COMPUTE_FUNC1_BCAST(popcountu32_compute_cpu, uint32_t, __builtin_popcount(A));
/* 4-wide u32 (GC3000) */
CPU_COMPUTE_FUNC1_MULTI(addu32_4w_compute_cpu, uint32_t, AI(i) + BI(i));
CPU_COMPUTE_FUNC1_MULTI(lshiftu32_4w_compute_cpu, uint32_t, AI(i) << (BI(i)&31));
CPU_COMPUTE_FUNC1_MULTI(rshiftu32_4w_compute_cpu, uint32_t, AI(i) >> (BI(i)&31));
CPU_COMPUTE_FUNC1_MULTI(rotateu32_4w_compute_cpu, uint32_t, (AI(i) << (BI(i)&31)) | (AI(i) >> ((32-BI(i))&31)));
CPU_COMPUTE_FUNC1_MULTI(oru32_4w_compute_cpu, uint32_t, AI(i) | BI(i));
CPU_COMPUTE_FUNC1_MULTI(andu32_4w_compute_cpu, uint32_t, AI(i) & BI(i));
CPU_COMPUTE_FUNC1_MULTI(xoru32_4w_compute_cpu, uint32_t, AI(i) ^ BI(i));
CPU_COMPUTE_FUNC1_MULTI(notu32_4w_compute_cpu, uint32_t, ~AI(i));
CPU_COMPUTE_FUNC1_MULTI(leadzerou32_4w_compute_cpu, uint32_t, (AI(i) != 0) ? __builtin_clz(AI(i)) : 0x20);
CPU_COMPUTE_FUNC1_MULTI(popcountu32_4w_compute_cpu, uint32_t, __builtin_popcount(AI(i)));
/* float */
CPU_COMPUTE_FUNC1_MULTI(addf32_compute_cpu, float, AI(i) + BI(i));
CPU_COMPUTE_FUNC1_MULTI(mulf32_compute_cpu, float, AI(i) * BI(i));
/* conversion between float and int (GC2000) */
CPU_COMPUTE_FUNC1_CVT_BCAST(f2i_s32_compute_cpu, int32_t, float, f2i_s32_gc2000(A));
CPU_COMPUTE_FUNC1_CVT_BCAST(f2i_u32_compute_cpu, uint32_t, float, f2i_u32_gc2000(A));
CPU_COMPUTE_FUNC1_CVT_BCAST(i2f_s32_compute_cpu, float, int32_t, A);
CPU_COMPUTE_FUNC1_CVT_BCAST(i2f_u32_compute_cpu, float, uint32_t, A);
/* 4-wide conversion (GC3000) - seems to match ARM semantics */
CPU_COMPUTE_FUNC1_CVT_MULTI(f2i_s32_4w_compute_cpu, int32_t, float, AI(i));
CPU_COMPUTE_FUNC1_CVT_MULTI(f2i_u32_4w_compute_cpu, uint32_t, float, AI(i));
CPU_COMPUTE_FUNC1_CVT_MULTI(i2f_s32_4w_compute_cpu, float, int32_t, AI(i));
CPU_COMPUTE_FUNC1_CVT_MULTI(i2f_u32_4w_compute_cpu, float, uint32_t, AI(i));
#undef A
#undef B
#undef AI
#undef BI
#undef CPU_COMPUTE

/* Tests GPU code must take from a[x] t2 and b[y] t3, and output to t4.
 * It can also take an ancillary argument in u3, taken from the auxin field.
 */
struct op_test op_tests[] = {
    {"nop", HWT_ALL, CT_INT32, i32_generate_values_h, i32_generate_values_v, (void*)nop_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x00000000, 0x00000000, 0x00000000, 0x00000000, /* nop */
        }))
    },
    /* Pretty much arbitrary test for multiple instructions */
    {"add4.u32", HWT_ALL, CT_INT32_BCAST, i32_generate_values_h, i32_generate_values_v, (void*)addu32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x00841001, 0x00202800, 0x80000000, 0x00000038, /* add.u32       t4.x___, t2.xxxx, void, t3.xxxx */
            0x01041001, 0x00202800, 0x80000000, 0x00000038, /* add.u32       t4._y__, t2.xxxx, void, t3.xxxx */
            0x02041001, 0x00202800, 0x80000000, 0x00000038, /* add.u32       t4.__z_, t2.xxxx, void, t3.xxxx */
            0x04041001, 0x00202800, 0x80000000, 0x00000038, /* add.u32       t4.___w, t2.xxxx, void, t3.xxxx */
        }))
    },
    /** These are scalar and broadcast the result on any known hw */
    {"imullo0.u32", HWT_ALL, CT_INT32_BCAST, i32_generate_values_h, i32_generate_values_v, (void*)mulu32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784103c, 0x39202800, 0x81c801c0, 0x00000000, /* imullo0.u32   t4, t2, t3, void */
        }))
    },
    {"imulhi0.u32", HWT_ALL, CT_INT32_BCAST, i32_generate_values_h, i32_generate_values_v, (void*)mulhu32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x07841000, 0x39202800, 0x81c901c0, 0x00000000, /* imulhi0.u32   t4, t2, t3, void */
        }))
    },
    {"imadlo0.u32", HWT_ALL, CT_INT32_BCAST, i32_generate_values_h, i32_generate_values_v, (void*)madu32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784100c, 0x39202800, 0x81c901c0, 0x20390038, /* imadlo0.u32   t4, t2, t3, u3 */
        })),
        {0x12345678, 0x0, 0x0, 0x0}
    },

    /** GC2000 behavior of bitwise instructions */
    {"add.u32", HWT_GC2000, CT_INT32_BCAST, i32_generate_values_h, i32_generate_values_v, (void*)addu32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x07841001, 0x39202800, 0x80000000, 0x00390038,  /* add.u32     t4, t2, void, t3 */
        }))
    },
    {"lshift.u32", HWT_GC2000, CT_INT32_BCAST, i32_generate_values_h, i32_generate_values_v, (void*)lshiftu32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x07841019, 0x39202800, 0x80010000, 0x00390038, /* lshift.u32    t4, t2, void, t3 */
        }))
    },
    {"rshift.u32", HWT_GC2000, CT_INT32_BCAST, i32_generate_values_h, i32_generate_values_v, (void*)rshiftu32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784101a, 0x39202800, 0x80010000, 0x00390038, /* rshift.u32    t4, t2, void, t3 */
        }))
    },
    {"rotate.u32", HWT_GC2000, CT_INT32_BCAST, i32_generate_values_h, i32_generate_values_v, (void*)rotateu32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784101b, 0x39202800, 0x80010000, 0x00390038, /* rotate.u32    t4, t2, void, t3 */
        }))
    },
    {"or.u32", HWT_GC2000, CT_INT32_BCAST, i32_generate_values_h, i32_generate_values_v, (void*)oru32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784101c, 0x39202800, 0x80010000, 0x00390038, /* or.u32        t4, t2, void, t3 */
        }))
    },
    {"and.u32", HWT_GC2000, CT_INT32_BCAST, i32_generate_values_h, i32_generate_values_v, (void*)andu32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784101d, 0x39202800, 0x80010000, 0x00390038, /* and.u32       t4, t2, void, t3 */
        }))
    },
    {"xor.u32", HWT_GC2000, CT_INT32_BCAST, i32_generate_values_h, i32_generate_values_v, (void*)xoru32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784101e, 0x39202800, 0x80010000, 0x00390038, /* xor.u32       t4, t2, void, t3 */
        }))
    },
    {"not.u32", HWT_GC2000, CT_INT32_BCAST, i32_generate_values_h, NULL, (void*)notu32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784101f, 0x00200000, 0x80010000, 0x00390028, /* not.u32       t4, void, void, t2 */
        }))
    },
    {"leadzero.u32", HWT_GC2000, CT_INT32_BCAST, i32_generate_values_h, NULL, (void*)leadzerou32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x07841018, 0x00200000, 0x80010000, 0x00390028, /* leadzero.u32  t4, void, void, t2 */
        }))
    },
    {"popcount.u32", HWT_GC2000, CT_INT32_BCAST, i32_generate_values_h, NULL, (void*)popcountu32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x07841021, 0x00200000, 0x80010000, 0x00390028, /* popcount.u32    t4, void, void, t2 */
        }))
    },

    /** Conversion instructions - GC2000 */
    {"f2i.s32", HWT_GC2000, CT_INT32_BCAST, i32_generate_values_h, NULL, (void*)f2i_s32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784102e, 0x39002800, 0x40000000, 0x00000000,  /* f2i.s32    t4, t2, void, void */
        })), {}
    },
    {"f2i.u32", HWT_GC2000, CT_INT32_BCAST, i32_generate_values_h, NULL, (void*)f2i_u32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784102e, 0x39202800, 0x80000000, 0x00000000,  /* f2i.u32     t4, t2, void, void */
        })), {}
    },
    /* Need to use "imprecise" float comparison here, as unlike on GC3000 the
       output will, for some values, be off-by-one compared to ARM.
    */
    {"i2f.s32", HWT_GC2000, CT_FLOAT32_BCAST, i32_generate_values_h, NULL, (void*)i2f_s32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784102d, 0x39002800, 0x40000000, 0x00000000,  /* i2f.s32     t4, t2, void, void */
        })), {}
    },
    {"i2f.u32", HWT_GC2000, CT_FLOAT32_BCAST, i32_generate_values_h, NULL, (void*)i2f_u32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784102d, 0x39202800, 0x80000000, 0x00000000,  /* i2f.u32     t4, t2, void, void */
        })), {}
    },

    /** GC3000 behavior of bitwise and some ALU instructions */
    {"add.u32", HWT_GC3000, CT_INT32, i32_generate_values_h, i32_generate_values_v, (void*)addu32_4w_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x07841001, 0x39202800, 0x80000000, 0x00390038,  /* add.u32     t4, t2, void, t3 */
        }))
    },
    {"lshift.u32", HWT_GC3000, CT_INT32, i32_generate_values_h4, i32_generate_values_v4, (void*)lshiftu32_4w_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x07841019, 0x39202800, 0x80010000, 0x00390038, /* lshift.u32    t4, t2, void, t3 */
        }))
    },
    {"rshift.u32", HWT_GC3000, CT_INT32, i32_generate_values_h4, i32_generate_values_v4, (void*)rshiftu32_4w_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784101a, 0x39202800, 0x80010000, 0x00390038, /* rshift.u32    t4, t2, void, t3 */
        }))
    },
    {"rotate.u32", HWT_GC3000, CT_INT32, i32_generate_values_h4, i32_generate_values_v4, (void*)rotateu32_4w_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784101b, 0x39202800, 0x80010000, 0x00390038, /* rotate.u32    t4, t2, void, t3 */
        }))
    },
    {"or.u32", HWT_GC3000, CT_INT32, i32_generate_values_h4, i32_generate_values_v4, (void*)oru32_4w_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784101c, 0x39202800, 0x80010000, 0x00390038, /* or.u32        t4, t2, void, t3 */
        }))
    },
    {"and.u32", HWT_GC3000, CT_INT32, i32_generate_values_h4, i32_generate_values_v4, (void*)andu32_4w_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784101d, 0x39202800, 0x80010000, 0x00390038, /* and.u32       t4, t2, void, t3 */
        }))
    },
    {"xor.u32", HWT_GC3000, CT_INT32, i32_generate_values_h4, i32_generate_values_v4, (void*)xoru32_4w_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784101e, 0x39202800, 0x80010000, 0x00390038, /* xor.u32       t4, t2, void, t3 */
        }))
    },
    {"not.u32", HWT_GC3000, CT_INT32, i32_generate_values_h4, NULL, (void*)notu32_4w_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784101f, 0x00200000, 0x80010000, 0x00390028, /* not.u32       t4, void, void, t2 */
        }))
    },
    {"leadzero.u32", HWT_GC3000, CT_INT32, i32_generate_values_h4, NULL, (void*)leadzerou32_4w_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x07841018, 0x00200000, 0x80010000, 0x00390028, /* leadzero.u32  t4, void, void, t2 */
        }))
    },
    {"popcount.u32", HWT_GC3000, CT_INT32, i32_generate_values_h, NULL, (void*)popcountu32_4w_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x07841021, 0x00200000, 0x80010000, 0x00390028, /* popcount.u32    t4, void, void, t2 */
        }))
    },

    /** Conversion instructions - GC3000 */
    {"f2i.s32", HWT_GC3000, CT_INT32, i32_generate_values_h, NULL, (void*)f2i_s32_4w_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784102e, 0x39002800, 0x40000000, 0x00000000,  /* f2i.s32    t4, t2, void, void */
        })), {}
    },
    {"f2i.u32", HWT_GC3000, CT_INT32, i32_generate_values_h, NULL, (void*)f2i_u32_4w_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784102e, 0x39202800, 0x80000000, 0x00000000,  /* f2i.u32     t4, t2, void, void */
        })), {}
    },
    {"i2f.s32", HWT_GC3000, CT_INT32, i32_generate_values_h, NULL, (void*)i2f_s32_4w_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784102d, 0x39002800, 0x40000000, 0x00000000,  /* i2f.s32     t4, t2, void, void */
        })), {}
    },
    {"i2f.u32", HWT_GC3000, CT_INT32, i32_generate_values_h, NULL, (void*)i2f_u32_4w_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x0784102d, 0x39202800, 0x80000000, 0x00000000,  /* i2f.u32     t4, t2, void, void */
        })), {}
    },
    // add.u16 does nothing

    /** Float ALU instructions */
    {"add.f32", HWT_ALL, CT_FLOAT32, i32_generate_values_h4, i32_generate_values_v4, (void*)addf32_compute_cpu,
        GPU_CODE(((uint32_t[]){
            0x07841001, 0x39002800, 0x00000000, 0x00390038, /* add           t4, t2, void, t3 */
        }))
    },
};

/* Compare 32-bit floating point values.
 * These are passed as integers to have more control over the comparison process.
 */
bool compare_float(uint32_t a, uint32_t b)
{
    if (abs(a-b) < 10) // Allow slightly different approximations
        return true;
    int a_sign = a>>31;
    int a_exponent = (a >> 23) & 0xff;
    int a_mantissa = a & 0x7fffff;
    int b_sign = b>>31;
    int b_exponent = (b >> 23) & 0xff;
    int b_mantissa = b & 0x7fffff;

    // allow large differences for really small values
    // these tend to be off or even clipped to 0
    if (a_exponent < 22 && b_exponent < 22)
        return abs(a_exponent-b_exponent) <= 1;

    // handle special values
    if (a_exponent == 0xff && b_exponent == 0xff) {
        if (a_mantissa == 0 && b_mantissa == 0) { // +Inf, -Inf
            return a_sign == b_sign;
        } else if (a_mantissa != 0 && b_mantissa != 0) { // NaN
            return true;
        }
    }

    if (a_exponent == 0x00 && b_exponent == 0x00) {
        // denormalized - just return true if same sign
        return a_sign == b_sign;
    }
    return false;
}

#define MAX_INST 1024
struct gpu_code *build_test_gpu_code(enum hardware_type hwt, struct op_test *test)
{
    uint32_t code[MAX_INST*4];
    unsigned code_ptr = 0;

    for (unsigned i=0; i<prelude.size; ++i)
        code[code_ptr++] = prelude.code[i];
    for (unsigned i=0; i<test->gpu_code.size; ++i)
        code[code_ptr++] = test->gpu_code.code[i];
    for (unsigned i=0; i<postlude.size; ++i)
        code[code_ptr++] = postlude.code[i];

    return gpu_code_new(code, code_ptr);
}

int perform_test(enum hardware_type hwt, struct drm_test_info *info, struct op_test *cur_test, int repeats)
{
    int retval = -1;
    const size_t unit_size = 16; /* vec4 of any 32-bit type */
    const size_t width = 16;
    const size_t height = 16;
    size_t seedx, seedy;
    struct etna_bo *bo_out=0, *bo_in0=0, *bo_in1=0;
    unsigned int errors = 0;

    size_t out_size = width * height * unit_size;
    size_t in0_size = width * unit_size;
    size_t in1_size = height * unit_size;

    void *out_cpu = malloc(out_size);
    void *a_cpu = malloc(in0_size);
    void *b_cpu = malloc(in1_size);
    struct gpu_code *test_code = NULL;
    memset(out_cpu, 0, out_size);
    memset(a_cpu, 0, in0_size);
    memset(b_cpu, 0, in1_size);

    printf("%s: ", cur_test->op_name);
    fflush(stdout);

    bo_out = etna_bo_new(info->dev, out_size, DRM_ETNA_GEM_CACHE_UNCACHED);
    bo_in0 = etna_bo_new(info->dev, in0_size, DRM_ETNA_GEM_CACHE_UNCACHED);
    bo_in1 = etna_bo_new(info->dev, in1_size, DRM_ETNA_GEM_CACHE_UNCACHED);
    if (!bo_in0 || !bo_in1 || !bo_out) {
        fprintf(stderr, "Unable to allocate buffer\n");
        goto out;
    }

    test_code = build_test_gpu_code(hwt, cur_test);
    if (hwt == HWT_GC3000) {
        gpu_code_alloc_bo(test_code, info->dev);
    }

    for (int num_tries=0; num_tries<repeats && !errors; ++num_tries) {
        seedx = rand();
        seedy = rand();
        cur_test->generate_values_h(seedx, a_cpu, width);
        if (cur_test->generate_values_v)
            cur_test->generate_values_v(seedy, b_cpu, height);
        cur_test->compute_cpu(out_cpu, a_cpu, b_cpu, width, height);

        memset(etna_bo_map(bo_out), 0, out_size);
        memcpy(etna_bo_map(bo_in0), a_cpu, in0_size);
        memcpy(etna_bo_map(bo_in1), b_cpu, in1_size);

        /* generate command sequence */
        gen_cmd_stream(hwt, info->stream, test_code, bo_out, bo_in0, bo_in1, cur_test->auxin);
        /* execute command sequence */
        etna_cmd_stream_finish(info->stream);

        /* verify result */
        const uint32_t *out_gpu = etna_bo_map(bo_out);
        if (cur_test->compare_type == CT_INT32 || cur_test->compare_type == CT_INT32_BCAST) {
            for(size_t y=0; y<height; ++y) {
                for(size_t x=0; x<width; ++x) {
                    for(size_t c=0; c<4; ++c) {
                        uint32_t expected = ((uint32_t*)out_cpu)[(y*width+x)*4 + c];
                        uint32_t found = ((uint32_t*)out_gpu)[(y*width+x)*4 + c];
                        if (expected != found) {
                            int sc = cur_test->compare_type == CT_INT32_BCAST ? 0 : c; /* source component */
                            if (errors < 10)
                                printf("Mismatch %s(%08x,%08x).%c -> %08x, expected %08x\n", cur_test->op_name,
                                        ((uint32_t*)a_cpu)[x*4 + sc], ((uint32_t*)b_cpu)[y*4 + sc],
                                        COMPS[c], found, expected);
                            errors += 1;
                        }
                    }
                }
            }
        } else if (cur_test->compare_type == CT_FLOAT32 || cur_test->compare_type == CT_FLOAT32_BCAST) {
            for(size_t y=0; y<height; ++y) {
                for(size_t x=0; x<width; ++x) {
                    for(size_t c=0; c<4; ++c) {
                        uint32_t expected = ((uint32_t*)out_cpu)[(y*width+x)*4 + c];
                        uint32_t found = ((uint32_t*)out_gpu)[(y*width+x)*4 + c];
                        if (!compare_float(expected, found)) {
                            int sc = cur_test->compare_type == CT_FLOAT32_BCAST ? 0 : c; /* source component */
                            if (errors < 10)
                                printf("Mismatch %s(%08x,%08x).%c -> %08x (%e), expected %08x (%e)\n", cur_test->op_name,
                                        ((uint32_t*)a_cpu)[x*4 + sc], ((uint32_t*)b_cpu)[y*4 + sc],
                                        COMPS[c], found, *(float*)&found, expected, *(float*)&expected);
                            errors += 1;
                        }
                    }
                }
            }
        } else {
            errors = 1;
            printf("No comparison implemented for compare_type %d\n", cur_test->compare_type);
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
    gpu_code_destroy(test_code);

    free(out_cpu);
    free(a_cpu);
    free(b_cpu);
    return retval;
}

int main(int argc, char *argv[])
{
    srand(time(NULL));
    struct drm_test_info *info;
    uint64_t val;
    enum hardware_type hwt = HWT_GC2000;
    if ((info = drm_test_setup(argc, argv)) == NULL) {
        return 1;
    }
    if (etna_gpu_get_param(info->gpu, ETNA_GPU_MODEL, &val)) {
        fprintf(stderr, "Could not get GPU model\n");
        goto error;
    }
    switch (val) {
    case 0x2000: printf("  Model: GC2000\n"); hwt = HWT_GC2000; break;
    case 0x3000: printf("  Model: GC3000\n"); hwt = HWT_GC3000; break;
    default:
        fprintf(stderr, "Do not know how to handle GPU model %08x\n", (uint32_t)val);
        goto error;
    }
    /* TODO real argument parsing */
    const char *only_test = NULL;
    int reps = 100;
    if (argc > 2) {
        only_test = argv[2];
        reps = 1000; /* do more rounds if running only one test */
    }
    for (unsigned t=0; t<ARRAY_SIZE(op_tests); ++t)
    {
        if (only_test && strcmp(only_test, op_tests[t].op_name))
            continue;
        if (op_tests[t].hardware_type & hwt) {
            perform_test(hwt, info, &op_tests[t], reps);
        }
    }

    drm_test_teardown(info);
    return 0;
error:
    drm_test_teardown(info);
    return 1;
}

