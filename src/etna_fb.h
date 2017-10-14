/*
 * Copyright (c) 2012-2017 Etnaviv Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
/* Utilities for framebuffer demos */
#ifndef H_FBDEMOS
#define H_FBDEMOS

#include <linux/fb.h>
#include <stddef.h>
#include <stdbool.h>

#include <etnaviv_drmif.h>

#define ETNA_FB_MAX_BUFFERS (2)
struct etna_device;
struct etna_bo;

struct fb_info
{
    int fd;
    int num_buffers;
    struct etna_bo *bo;
    struct etna_reloc buffer[ETNA_FB_MAX_BUFFERS];
    int width, height;
    int padded_height;
    size_t stride;
    size_t buffer_stride;
    struct fb_var_screeninfo fb_var;
    struct fb_fix_screeninfo fb_fix;

    /* Resolve format (-1 if no match), and swap red/blue bit */
    int rs_format;
    bool swap_rb;
};

/* Open framebuffer and get information */
int fb_open(struct etna_device *conn, int num, struct fb_info **out_p);

/* Set currently visible buffer id */
int fb_set_buffer(struct fb_info *fb, int buffer);

/* Close framebuffer */
int fb_close(struct fb_info *fb);

#endif
