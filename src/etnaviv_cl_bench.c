/*
 * Copyright (C) 2016 Etnaviv Project.
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
/* CL benchmark framework */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define _POSIX_C_SOURCE 199309L

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include <assert.h>
#include <stdbool.h>

#include "drm_setup.h"
#include "cmdstream.h"
#include "gpu_code.h"

#include "state.xml.h"
#include "state_3d.xml.h"
#include "common.xml.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* GPU code for write benchmark */
#define WRITE_SIZE (8192*16) /* 128 kiB */
#define WRITE_INNERREPS 1000
struct gpu_code write_bench_code_gc3000 = GPU_CODE(((uint32_t[]){
/*   0: */ 0x00801009, 0x00000000, 0x00000000, 0x74000008,  /* mov	t0.x___, void, void, ?7?0[a.y].xxxx */
/*   1: */ 0x000000d6, 0x00000800, 0x5002f440, 0x780000ef,  /* branch.ge.s32	void, t0.xxxx, ?7?488[a.y].yxxx, 1 */
/*   2: */ 0x01001009, 0x00000000, 0x00000000, 0x20000008,  /* mov	t0._y__, void, void, u0.xxxx */
/*   3: */ 0x02001009, 0x00000000, 0x00000000, 0x74000008,  /* mov	t0.__z_, void, void, ?7?0[a.y].xxxx */
/*   4: */ 0x000000d6, 0x2a800800, 0x50400040, 0x780000cf,  /* branch.ge.s32	void, t0.zzzz, ?7?0[a.y].xxzx, 1 */
/*   5: */ 0x07810033, 0x15600c00, 0x80aa0040, 0x78000aaa,  /* store.u32	mem.xyzw, t0.yyyy, u0.yyyy, ?7?170[a.w].xxxx */
/*   6: */ 0x07810033, 0x15600c00, 0x90000840, 0x78000aaf,  /* store.u32	mem.xyzw, t0.yyyy, ?7?16[a.y].xxxx, ?7?170[a.w].xxxx */
/*   7: */ 0x07810033, 0x15600c00, 0x90001040, 0x78000aaf,  /* store.u32	mem.xyzw, t0.yyyy, ?7?32[a.y].xxxx, ?7?170[a.w].xxxx */
/*   8: */ 0x07810033, 0x15600c00, 0x90001840, 0x78000aaf,  /* store.u32	mem.xyzw, t0.yyyy, ?7?48[a.y].xxxx, ?7?170[a.w].xxxx */
/*   9: */ 0x01001001, 0x15600800, 0x80000000, 0x74000408,  /* add.u32	t0._y__, t0.yyyy, void, ?7?64[a.y].xxxx */
/*  10: */ 0x02001001, 0x2a800800, 0x40000000, 0x74000048,  /* add.s32	t0.__z_, t0.zzzz, void, ?7?4[a.y].xxxx */
/*  11: */ 0x00000096, 0x2a800800, 0x50200040, 0x7800005f,  /* branch.lt.s32	void, t0.zzzz, ?7?0[a.y].xxzx, 0 */
/*  12: */ 0x00801001, 0x00000800, 0x40000000, 0x74000018,  /* add.s32	t0.x___, t0.xxxx, void, ?7?1[a.y].xxxx */
/*  13: */ 0x00000096, 0x00000800, 0x5002f440, 0x7800002f,  /* branch.lt.s32	void, t0.xxxx, ?7?488[a.y].yxxx, 0 */
/*  14: */ 0x00000000, 0x00000000, 0x00000000, 0x00000000,  /* nop	void, void, void, void */
}));

struct gpu_code write_bench_code_gc2000 = GPU_CODE_UNIFORMS(((uint32_t[]){
/*   0: */ 0x00801009, 0x00000000, 0x00000000, 0x20154008,  /* mov	t0.x___, void, void, u0.yyyy */
/*   1: */ 0x000000d6, 0x00000800, 0x41540040, 0x00000782,  /* branch.ge.s32	void, t0.xxxx, u0.zzzz, 15 */
/*   2: */ 0x01001009, 0x00000000, 0x00000000, 0x20000008,  /* mov	t0._y__, void, void, u0.xxxx */
/*   3: */ 0x02001009, 0x00000000, 0x00000000, 0x20154008,  /* mov	t0.__z_, void, void, u0.yyyy */
/*   4: */ 0x000000d6, 0x2a800800, 0x41fe0040, 0x00000682,  /* branch.ge.s32	void, t0.zzzz, u0.wwww, 13 */
/*   5: */ 0x07831009, 0x00000000, 0x00000000, 0x20390008,  /* mov	t3, void, void, u0 */
/*   6: */ 0x07820033, 0x15600800, 0x80aa01c0, 0x20000018,  /* store.u32	mem.xyzw, t0.yyyy, t3.yyyy, u1.xxxx */
/*   7: */ 0x07820033, 0x15600800, 0x80aa00c0, 0x202a801a,  /* store.u32	mem.xyzw, t0.yyyy, u1.yyyy, u1.zzzz */
/*   8: */ 0x07820033, 0x15600800, 0x80000140, 0x2015402a,  /* store.u32	mem.xyzw, t0.yyyy, u2.xxxx, u2.yyyy */
/*   9: */ 0x07820033, 0x15600800, 0x81540140, 0x203fc02a,  /* store.u32	mem.xyzw, t0.yyyy, u2.zzzz, u2.wwww */
/*  10: */ 0x01001001, 0x15600800, 0x80000000, 0x203fc018,  /* add.u32	t0._y__, t0.yyyy, void, u1.wwww */
/*  11: */ 0x02001001, 0x2a800800, 0x40000000, 0x20000038,  /* add.s32	t0.__z_, t0.zzzz, void, u3.xxxx */
/*  12: */ 0x00000096, 0x2a800800, 0x41fe0040, 0x00000282,  /* branch.lt.s32	void, t0.zzzz, u0.wwww, 5 */
/*  13: */ 0x00801001, 0x00000800, 0x40000000, 0x20154038,  /* add.s32	t0.x___, t0.xxxx, void, u3.yyyy */
/*  14: */ 0x00000096, 0x00000800, 0x41540040, 0x00000102,  /* branch.lt.s32	void, t0.xxxx, u0.zzzz, 2 */
/*  15: */ 0x00000000, 0x00000000, 0x00000000, 0x00000000,  /* nop	void, void, void, void */
}),
((uint32_t[]){ /* u0.x takes address */
    0x0, 0x0, 0x3e8, 0x2000, 0xaa, 0x10, 0xaa, 0x40, 0x20, 0xaa, 0x30, 0xaa, 0x4, 0x1,
}));

/* GPU code for read benchmark */
#define READ_SIZE (8192*16) /* 128 kiB */
#define READ_INNERREPS 1000
struct gpu_code read_bench_code_gc3000 = GPU_CODE(((uint32_t[]){
/*   0: */ 0x00801009, 0x00000000, 0x00000000, 0x74000008,  /* mov	t0.x___, void, void, ?7?0[a.y].xxxx */
/*   1: */ 0x000000d6, 0x00000800, 0x5002f440, 0x780000ef,  /* branch.ge.s32	void, t0.xxxx, ?7?488[a.y].yxxx, 1 */
/*   2: */ 0x01001009, 0x00000000, 0x00000000, 0x20000008,  /* mov	t0._y__, void, void, u0.xxxx */
/*   3: */ 0x02001009, 0x00000000, 0x00000000, 0x74000008,  /* mov	t0.__z_, void, void, ?7?0[a.y].xxxx */
/*   4: */ 0x000000d6, 0x2a800800, 0x50400040, 0x780000cf,  /* branch.ge.s32	void, t0.zzzz, ?7?0[a.y].xxzx, 1 */
/*   5: */ 0x07821032, 0x15600c00, 0x90000040, 0x00000007,  /* load.u32	t2, t0.yyyy, ?7?0[a.y].xxxx, void */
/*   6: */ 0x07821032, 0x15600c00, 0x90000840, 0x00000007,  /* load.u32	t2, t0.yyyy, ?7?16[a.y].xxxx, void */
/*   7: */ 0x07821032, 0x15600c00, 0x90001040, 0x00000007,  /* load.u32	t2, t0.yyyy, ?7?32[a.y].xxxx, void */
/*   8: */ 0x07821032, 0x15600c00, 0x90001840, 0x00000007,  /* load.u32	t2, t0.yyyy, ?7?48[a.y].xxxx, void */
/*   9: */ 0x01001001, 0x15600800, 0x80000000, 0x74000408,  /* add.u32	t0._y__, t0.yyyy, void, ?7?64[a.y].xxxx */
/*  10: */ 0x02001001, 0x2a800800, 0x40000000, 0x74000048,  /* add.s32	t0.__z_, t0.zzzz, void, ?7?4[a.y].xxxx */
/*  11: */ 0x00000096, 0x2a800800, 0x50200040, 0x7800005f,  /* branch.lt.s32	void, t0.zzzz, ?7?0[a.y].xxzx, 0 */
/*  12: */ 0x00801001, 0x00000800, 0x40000000, 0x74000018,  /* add.s32	t0.x___, t0.xxxx, void, ?7?1[a.y].xxxx */
/*  13: */ 0x00000096, 0x00000800, 0x5002f440, 0x7800002f,  /* branch.lt.s32	void, t0.xxxx, ?7?488[a.y].yxxx, 0 */
/*  14: */ 0x00000000, 0x00000000, 0x00000000, 0x00000000,  /* nop	void, void, void, void */
}));

struct gpu_code read_bench_code_gc2000 = GPU_CODE_UNIFORMS(((uint32_t[]){
/*   0: */ 0x00821009, 0x00000000, 0x00000000, 0x20154008,  /* mov	t2.x___, void, void, u0.yyyy */
/*   1: */ 0x000000d6, 0x00002800, 0x41540040, 0x00000682,  /* branch.ge.s32	void, t2.xxxx, u0.zzzz, 13 */
/*   2: */ 0x03021009, 0x00000000, 0x00000000, 0x20040008,  /* mov	t2._yz_, void, void, u0.xxyx */
/*   3: */ 0x000000d6, 0x2a802800, 0x41fe0040, 0x00000582,  /* branch.ge.s32	void, t2.zzzz, u0.wwww, 11 */
/*   4: */ 0x07811032, 0x15602800, 0x80aa0040, 0x00000002,  /* load.u32	t1, t2.yyyy, u0.yyyy, void */
/*   5: */ 0x07811032, 0x15602800, 0x800000c0, 0x00000002,  /* load.u32	t1, t2.yyyy, u1.xxxx, void */
/*   6: */ 0x07811032, 0x15602800, 0x80aa00c0, 0x00000002,  /* load.u32	t1, t2.yyyy, u1.yyyy, void */
/*   7: */ 0x07811032, 0x15602800, 0x815400c0, 0x00000002,  /* load.u32	t1, t2.yyyy, u1.zzzz, void */
/*   8: */ 0x01021001, 0x15602800, 0x80000000, 0x203fc018,  /* add.u32	t2._y__, t2.yyyy, void, u1.wwww */
/*   9: */ 0x02021001, 0x2a802800, 0x40000000, 0x20000028,  /* add.s32	t2.__z_, t2.zzzz, void, u2.xxxx */
/*  10: */ 0x00000096, 0x2a802800, 0x41fe0040, 0x00000202,  /* branch.lt.s32	void, t2.zzzz, u0.wwww, 4 */
/*  11: */ 0x00821001, 0x00002800, 0x40000000, 0x20154028,  /* add.s32	t2.x___, t2.xxxx, void, u2.yyyy */
/*  12: */ 0x00000096, 0x00002800, 0x41540040, 0x00000102,  /* branch.lt.s32	void, t2.xxxx, u0.zzzz, 2 */
/*  13: */ 0x00000000, 0x00000000, 0x00000000, 0x00000000,  /* nop	void, void, void, void */
}),
((uint32_t[]){ /* u0.x takes address */
    0x0, 0x0, 0x3e8, 0x2000, 0x10, 0x20, 0x30, 0x40, 0x4, 0x1,
}));

/* GPU code for read+acc benchmark */
#define READACC_SIZE (8192*16) /* 128 kiB */
#define READACC_INNERREPS 1000
struct gpu_code readacc_bench_code_gc3000 = GPU_CODE(((uint32_t[]){
/*   0: */ 0x07801009, 0x00000000, 0x00000000, 0x78000008,  /* mov	t0, void, void, ?7?0[a.w].xxxx */
/*   1: */ 0x00811009, 0x00000000, 0x00000000, 0x74000008,  /* mov	t1.x___, void, void, ?7?0[a.y].xxxx */
/*   2: */ 0x000000d6, 0x00001800, 0x5002f440, 0x7800013f,  /* branch.ge.s32	void, t1.xxxx, ?7?488[a.y].yxxx, 2 */
/*   3: */ 0x01011009, 0x00000000, 0x00000000, 0x20000008,  /* mov	t1._y__, void, void, u0.xxxx */
/*   4: */ 0x02011009, 0x00000000, 0x00000000, 0x74000008,  /* mov	t1.__z_, void, void, ?7?0[a.y].xxxx */
/*   5: */ 0x000000d6, 0x2a801800, 0x50400040, 0x7800011f,  /* branch.ge.s32	void, t1.zzzz, ?7?0[a.y].xxzx, 2 */
/*   6: */ 0x07821032, 0x15601c00, 0x90000040, 0x00000007,  /* load.u32	t2, t1.yyyy, ?7?0[a.y].xxxx, void */
/*   7: */ 0x07801001, 0x39200800, 0x80000000, 0x00390028,  /* add.u32	t0, t0, void, t2 */
/*   8: */ 0x07821032, 0x15601c00, 0x90000840, 0x00000007,  /* load.u32	t2, t1.yyyy, ?7?16[a.y].xxxx, void */
/*   9: */ 0x07801001, 0x39200800, 0x80000000, 0x00390028,  /* add.u32	t0, t0, void, t2 */
/*  10: */ 0x07821032, 0x15601c00, 0x90001040, 0x00000007,  /* load.u32	t2, t1.yyyy, ?7?32[a.y].xxxx, void */
/*  11: */ 0x07801001, 0x39200800, 0x80000000, 0x00390028,  /* add.u32	t0, t0, void, t2 */
/*  12: */ 0x07821032, 0x15601c00, 0x90001840, 0x00000007,  /* load.u32	t2, t1.yyyy, ?7?48[a.y].xxxx, void */
/*  13: */ 0x07801001, 0x39200800, 0x80000000, 0x00390028,  /* add.u32	t0, t0, void, t2 */
/*  14: */ 0x01011001, 0x15601800, 0x80000000, 0x74000408,  /* add.u32	t1._y__, t1.yyyy, void, ?7?64[a.y].xxxx */
/*  15: */ 0x02011001, 0x2a801800, 0x40000000, 0x74000048,  /* add.s32	t1.__z_, t1.zzzz, void, ?7?4[a.y].xxxx */
/*  16: */ 0x00000096, 0x2a801800, 0x50200040, 0x7800006f,  /* branch.lt.s32	void, t1.zzzz, ?7?0[a.y].xxzx, 0 */
/*  17: */ 0x00811001, 0x00001800, 0x40000000, 0x74000018,  /* add.s32	t1.x___, t1.xxxx, void, ?7?1[a.y].xxxx */
/*  18: */ 0x00000096, 0x00001800, 0x5002f440, 0x7800003f,  /* branch.lt.s32	void, t1.xxxx, ?7?488[a.y].yxxx, 0 */
/*  19: */ 0x00000000, 0x00000000, 0x00000000, 0x00000000,  /* nop	void, void, void, void */
}));

struct gpu_code readacc_bench_code_gc2000 = GPU_CODE_UNIFORMS(((uint32_t[]){
/*   0: */ 0x07801009, 0x00000000, 0x00000000, 0x20154008,  /* mov	t0, void, void, u0.yyyy */
/*   1: */ 0x00821009, 0x00000000, 0x00000000, 0x20154008,  /* mov	t2.x___, void, void, u0.yyyy */
/*   2: */ 0x000000d6, 0x00002800, 0x41540040, 0x00001002,  /* branch.ge.s32	void, t2.xxxx, u0.zzzz, 32 */
/*   3: */ 0x03021009, 0x00000000, 0x00000000, 0x20040008,  /* mov	t2._yz_, void, void, u0.xxyx */
/*   4: */ 0x000000d6, 0x2a802800, 0x41fe0040, 0x00000f02,  /* branch.ge.s32	void, t2.zzzz, u0.wwww, 30 */
/*   5: */ 0x07811032, 0x15602800, 0x80aa0040, 0x00000002,  /* load.u32	t1, t2.yyyy, u0.yyyy, void */
/*   6: */ 0x07831009, 0x00000000, 0x00000000, 0x00390018,  /* mov	t3, void, void, t1 */
/*   7: */ 0x00801001, 0x00200800, 0x80000000, 0x00000038,  /* add.u32	t0.x___, t0.xxxx, void, t3.xxxx */
/*   8: */ 0x01001001, 0x15600800, 0x80000000, 0x00154038,  /* add.u32	t0._y__, t0.yyyy, void, t3.yyyy */
/*   9: */ 0x02001001, 0x2aa00800, 0x80000000, 0x002a8038,  /* add.u32	t0.__z_, t0.zzzz, void, t3.zzzz */
/*  10: */ 0x04001001, 0x3fe00800, 0x80000000, 0x003fc038,  /* add.u32	t0.___w, t0.wwww, void, t3.wwww */
/*  11: */ 0x07811032, 0x15602800, 0x800000c0, 0x00000002,  /* load.u32	t1, t2.yyyy, u1.xxxx, void */
/*  12: */ 0x00801001, 0x00200800, 0x80000000, 0x00000018,  /* add.u32	t0.x___, t0.xxxx, void, t1.xxxx */
/*  13: */ 0x01001001, 0x15600800, 0x80000000, 0x00154018,  /* add.u32	t0._y__, t0.yyyy, void, t1.yyyy */
/*  14: */ 0x02001001, 0x2aa00800, 0x80000000, 0x002a8018,  /* add.u32	t0.__z_, t0.zzzz, void, t1.zzzz */
/*  15: */ 0x04001001, 0x3fe00800, 0x80000000, 0x003fc018,  /* add.u32	t0.___w, t0.wwww, void, t1.wwww */
/*  16: */ 0x07811032, 0x15602800, 0x80aa00c0, 0x00000002,  /* load.u32	t1, t2.yyyy, u1.yyyy, void */
/*  17: */ 0x00801001, 0x00200800, 0x80000000, 0x00000018,  /* add.u32	t0.x___, t0.xxxx, void, t1.xxxx */
/*  18: */ 0x01001001, 0x15600800, 0x80000000, 0x00154018,  /* add.u32	t0._y__, t0.yyyy, void, t1.yyyy */
/*  19: */ 0x02001001, 0x2aa00800, 0x80000000, 0x002a8018,  /* add.u32	t0.__z_, t0.zzzz, void, t1.zzzz */
/*  20: */ 0x04001001, 0x3fe00800, 0x80000000, 0x003fc018,  /* add.u32	t0.___w, t0.wwww, void, t1.wwww */
/*  21: */ 0x07811032, 0x15602800, 0x815400c0, 0x00000002,  /* load.u32	t1, t2.yyyy, u1.zzzz, void */
/*  22: */ 0x07831009, 0x00000000, 0x00000000, 0x00390018,  /* mov	t3, void, void, t1 */
/*  23: */ 0x00801001, 0x00200800, 0x80000000, 0x00000038,  /* add.u32	t0.x___, t0.xxxx, void, t3.xxxx */
/*  24: */ 0x01001001, 0x15600800, 0x80000000, 0x00154038,  /* add.u32	t0._y__, t0.yyyy, void, t3.yyyy */
/*  25: */ 0x02001001, 0x2aa00800, 0x80000000, 0x002a8038,  /* add.u32	t0.__z_, t0.zzzz, void, t3.zzzz */
/*  26: */ 0x04001001, 0x3fe00800, 0x80000000, 0x003fc038,  /* add.u32	t0.___w, t0.wwww, void, t3.wwww */
/*  27: */ 0x01021001, 0x15602800, 0x80000000, 0x203fc018,  /* add.u32	t2._y__, t2.yyyy, void, u1.wwww */
/*  28: */ 0x02021001, 0x2a802800, 0x40000000, 0x20000028,  /* add.s32	t2.__z_, t2.zzzz, void, u2.xxxx */
/*  29: */ 0x00000096, 0x2a802800, 0x41fe0040, 0x00000282,  /* branch.lt.s32	void, t2.zzzz, u0.wwww, 5 */
/*  30: */ 0x00821001, 0x00002800, 0x40000000, 0x20154028,  /* add.s32	t2.x___, t2.xxxx, void, u2.yyyy */
/*  31: */ 0x00000096, 0x00002800, 0x41540040, 0x00000182,  /* branch.lt.s32	void, t2.xxxx, u0.zzzz, 3 */
/*  32: */ 0x00000000, 0x00000000, 0x00000000, 0x00000000,  /* nop	void, void, void, void */
}),
((uint32_t[]){ /* u0.x takes address */
    0x0, 0x0, 0x3e8, 0x2000, 0x10, 0x20, 0x30, 0x40, 0x4, 0x1,
}));

static void gen_cmd_stream_gc3000(struct etna_cmd_stream *stream, struct gpu_code *gpu_code,
        struct etna_bo *bmp, uint32_t out_gpu_addr)
{
    etna_set_state(stream, VIVS_PA_SYSTEM_MODE, VIVS_PA_SYSTEM_MODE_UNK0 | VIVS_PA_SYSTEM_MODE_UNK4);
    etna_set_state(stream, VIVS_GL_API_MODE, VIVS_GL_API_MODE_OPENCL);
    etna_set_state(stream, VIVS_VS_UNIFORM_CACHE, 0x1000);

    etna_set_state(stream, VIVS_PS_INPUT_COUNT, VIVS_PS_INPUT_COUNT_COUNT(1) | VIVS_PS_INPUT_COUNT_UNK8(31));
    etna_set_state(stream, VIVS_PS_TEMP_REGISTER_CONTROL, VIVS_PS_TEMP_REGISTER_CONTROL_NUM_TEMPS(4));
    etna_set_state(stream, VIVS_VS_OUTPUT_COUNT, 0);
    etna_set_state(stream, VIVS_CL_UNK00924, 0x0);
    etna_set_state(stream, VIVS_VS_UNIFORM_CACHE, 0x1011);

    for (unsigned i=0; i<gpu_code->uniforms_size; ++i)
        etna_set_state(stream, VIVS_SH_UNIFORMS(i), gpu_code->uniforms[i]);

    if (bmp) {
        etna_set_state_from_bo(stream, VIVS_SH_UNIFORMS(0), bmp, ETNA_RELOC_WRITE);
    } else {
        etna_set_state(stream, VIVS_SH_UNIFORMS(0), out_gpu_addr);
    }
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
    etna_set_state(stream, VIVS_PS_RANGE, VIVS_PS_RANGE_LOW(0x0) | VIVS_PS_RANGE_HIGH(gpu_code->size/4-2));
    assert(gpu_code->bo);
    etna_set_state_from_bo(stream, VIVS_PS_INST_ADDR, gpu_code->bo, ETNA_RELOC_READ);
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

static void gen_cmd_stream_gc2000(struct etna_cmd_stream *stream, struct gpu_code *gpu_code,
        struct etna_bo *bmp, uint32_t out_gpu_addr)
{
    etna_set_state(stream, VIVS_PA_SYSTEM_MODE, VIVS_PA_SYSTEM_MODE_UNK0 | VIVS_PA_SYSTEM_MODE_UNK4);
    etna_set_state(stream, VIVS_GL_API_MODE, VIVS_GL_API_MODE_OPENCL);

    etna_set_state(stream, VIVS_VS_INPUT_COUNT, VIVS_VS_INPUT_COUNT_COUNT(1) | VIVS_VS_INPUT_COUNT_UNK8(31));
    etna_set_state(stream, VIVS_VS_INPUT(0), VIVS_VS_INPUT_I0(0) | VIVS_VS_INPUT_I1(1) | VIVS_VS_INPUT_I2(2) | VIVS_VS_INPUT_I3(3));
    etna_set_state(stream, VIVS_VS_TEMP_REGISTER_CONTROL, VIVS_VS_TEMP_REGISTER_CONTROL_NUM_TEMPS(10));
    etna_set_state(stream, VIVS_VS_OUTPUT_COUNT, 0);
    etna_set_state(stream, VIVS_CL_UNK00924, 0x0);

    for (unsigned i=0; i<gpu_code->uniforms_size; ++i)
        etna_set_state(stream, VIVS_VS_UNIFORMS(i), gpu_code->uniforms[i]);

    if (bmp) {
        etna_set_state_from_bo(stream, VIVS_VS_UNIFORMS(0), bmp, ETNA_RELOC_WRITE);
    } else {
        etna_set_state(stream, VIVS_VS_UNIFORMS(0), out_gpu_addr);
    }

    etna_stall(stream, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);

    etna_set_state(stream, VIVS_VS_INPUT_COUNT, VIVS_VS_INPUT_COUNT_COUNT(1) | VIVS_VS_INPUT_COUNT_UNK8(1));
    etna_set_state(stream, VIVS_VS_TEMP_REGISTER_CONTROL, VIVS_VS_TEMP_REGISTER_CONTROL_NUM_TEMPS(10));
    etna_set_state(stream, VIVS_VS_OUTPUT(0), VIVS_VS_OUTPUT_O0(0) | VIVS_VS_OUTPUT_O1(0) | VIVS_VS_OUTPUT_O2(0) | VIVS_VS_OUTPUT_O3(0));
    etna_set_state(stream, VIVS_GL_VARYING_NUM_COMPONENTS, VIVS_GL_VARYING_NUM_COMPONENTS_VAR0(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR1(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR2(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR3(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR4(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR5(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR6(0x0) | VIVS_GL_VARYING_NUM_COMPONENTS_VAR7(0x0));
    etna_set_state(stream, VIVS_GL_UNK03834, 0x0);
    etna_set_state(stream, VIVS_VS_UNIFORM_CACHE, 0);
    etna_set_state(stream, VIVS_VS_RANGE, VIVS_VS_RANGE_LOW(0x0) | VIVS_VS_RANGE_HIGH(gpu_code->size/4-2));

    for (unsigned i=0; i<gpu_code->size; ++i)
        etna_set_state(stream, VIVS_SH_INST_MEM(i), gpu_code->code[i]);

    etna_set_state(stream, VIVS_PS_INPUT_COUNT, VIVS_PS_INPUT_COUNT_COUNT(1) | VIVS_PS_INPUT_COUNT_UNK8(31));
    etna_set_state(stream, VIVS_PS_TEMP_REGISTER_CONTROL, VIVS_PS_TEMP_REGISTER_CONTROL_NUM_TEMPS(10));
    etna_set_state(stream, VIVS_PS_CONTROL, 0);
    etna_set_state(stream, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT, VIVS_PA_ATTRIBUTE_ELEMENT_COUNT_UNK0(0x0) | VIVS_PA_ATTRIBUTE_ELEMENT_COUNT_COUNT(0x0));
    etna_set_state(stream, VIVS_GL_VARYING_TOTAL_COMPONENTS, VIVS_GL_VARYING_TOTAL_COMPONENTS_NUM(0x0));
    etna_set_state(stream, VIVS_PS_CONTROL_EXT, VIVS_PS_CONTROL_EXT_COLOR_OUTPUT_COUNT(0x0));
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

    etna_stall(stream, SYNC_RECIPIENT_FE, SYNC_RECIPIENT_PE);

    etna_set_state(stream, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_DEPTH | VIVS_GL_FLUSH_CACHE_COLOR);
}

static const uint32_t OCRAM_BASE = 0x00900000;
static const uint32_t OCRAM_SIZE = 0x00080000;
/* This assumes that the kernel mapped the OCRAM here in GPU address space */
static const uint32_t OCRAM_GPU_BASE = 0xf8000000;

int main(int argc, char *argv[])
{
    bool use_ocram = false;
    uint8_t *ocram_ptr = NULL;
    struct drm_test_info *info;
    struct etna_bo *bmp;
    if ((info = drm_test_setup(argc, argv)) == NULL) {
        return 1;
    }
    enum hardware_type hwt = drm_cl_get_hardware_type(info);
    if (hwt == HWT_OTHER) {
        return 1;
    }
    if (hwt != HWT_GC3000) {
        use_ocram = false;
    }

    if (use_ocram) {
        int fd = open("/dev/mem", O_RDWR);
        if (fd < 0) {
            perror("open /dev/mem");
            return 1;
        }
        ocram_ptr = mmap(NULL, OCRAM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, OCRAM_BASE);
        if (ocram_ptr == 0) {
            perror("map ocram");
            return 1;
        }

        /* check it */
        for (unsigned i=0; i<OCRAM_SIZE; ++i)
            ocram_ptr[i] = i ^ (i>>8) ^ (i>>16);
        for (unsigned i=0; i<OCRAM_SIZE; ++i) {
            if (ocram_ptr[i] != (uint8_t)(i ^ (i>>8) ^ (i>>16))) {
                fprintf(stderr, "OCRAM validation failed at %08x\n", OCRAM_BASE+ i);
                return 1;
            }
        }

        /* initial state: clear */
        memset(ocram_ptr, 0, OCRAM_SIZE);
    }

    bmp = etna_bo_new(info->dev, OCRAM_SIZE, DRM_ETNA_GEM_CACHE_UNCACHED);
    if (!bmp) {
        fprintf(stderr, "Unable to allocate buffer\n");
        goto out;
    }
    for (int memtype=0; memtype<4; ++memtype) {
        for (int benchtype=0; benchtype<3; ++benchtype) {
            size_t scale;
            struct gpu_code *code = NULL;
            if (memtype > 0 && !use_ocram)
                continue;
            switch (benchtype) {
            case 0:
                code = (hwt==HWT_GC3000) ? &read_bench_code_gc3000 : &read_bench_code_gc2000;
                scale = READ_SIZE * READ_INNERREPS;
                printf("[read]    ");
                break;
            case 1:
                code = (hwt==HWT_GC3000) ? &readacc_bench_code_gc3000 : &readacc_bench_code_gc2000;
                scale = READACC_SIZE * READACC_INNERREPS;
                printf("[readacc] ");
                break;
            case 2:
                code = (hwt==HWT_GC3000) ? &write_bench_code_gc3000 : &write_bench_code_gc2000;
                scale = WRITE_SIZE * WRITE_INNERREPS;
                printf("[write]   ");
                break;
            }

            /* generate command sequence */
            struct etna_bo *bo_out = NULL;
            uint32_t addr_out = 0;
            switch(memtype) {
            case 0:
                printf("[ram]    ");
                bo_out = bmp;
                break;
            case 1:
                printf("[ocram1] ");
                addr_out = OCRAM_GPU_BASE;
                break;
            case 2:
                printf("[ocram2] ");
                addr_out = OCRAM_GPU_BASE + 0x40000;
                break;
            case 3:
                printf("[ocram3] ");
                addr_out = OCRAM_GPU_BASE + 0x60000;
                break;
            }
            switch(hwt) {
            case HWT_GC2000:
                gen_cmd_stream_gc2000(info->stream, code, bo_out, addr_out);
                break;
            case HWT_GC3000:
                gpu_code_alloc_bo(code, info->dev);
                gen_cmd_stream_gc3000(info->stream, code, bo_out, addr_out);
                break;
            default: assert(0);
            }

            struct timespec tp_start, tp_end;
            clock_gettime(CLOCK_MONOTONIC, &tp_start);

            etna_cmd_stream_finish(info->stream);

            clock_gettime(CLOCK_MONOTONIC, &tp_end);

            int reps = 1;
            double elapsed = ((double)tp_end.tv_sec - (double)tp_start.tv_sec + ((double)tp_end.tv_nsec - (double)tp_start.tv_nsec)/1e9) / reps;
            printf("Speed: %7.3f MB/s\n", (scale / elapsed)/1e6);
        }
    }

    if (use_ocram) {
        const unsigned char *data = ocram_ptr;
        for(int i=0; i<0x100; ++i) {
            printf("%02x ", data[i]);
        }
        printf("\n");
        printf("%s\n", data);
    }

    drm_test_teardown(info);
    return 0;
out:
    drm_test_teardown(info);
    return 1;
}

