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
#define _POSIX_C_SOURCE 200112L
#include "etna_fb.h"
#include "etna_util.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <string.h>
#include <linux/videodev2.h>
#include <errno.h>

#include <state_3d.xml.h>

#ifdef ANDROID
#define FBDEV_DEV "/dev/graphics/fb%i"
#else
#define FBDEV_DEV "/dev/fb%i"
#endif

/* Structure to convert framebuffer format to RS destination conf */
struct etna_fb_format_desc
{
    unsigned bits_per_pixel;
    unsigned red_offset;
    unsigned red_length;
    unsigned green_offset;
    unsigned green_length;
    unsigned blue_offset;
    unsigned blue_length;
    unsigned alpha_offset;
    unsigned alpha_length;
    unsigned grayscale;
    unsigned rs_format;
    bool swap_rb;
};

static const struct etna_fb_format_desc etna_fb_formats[] = {
 /* bpp  ro  rl go gl bo  bl ao  al gs rs_format           swap_rb */
    {32, 16, 8, 8, 8, 0 , 8, 0,  0, 0, RS_FORMAT_X8R8G8B8, false},
    {32, 0 , 8, 8, 8, 16, 8, 0,  0, 0, RS_FORMAT_X8R8G8B8, true},
    {32, 16, 8, 8, 8, 0 , 8, 24, 8, 0, RS_FORMAT_A8R8G8B8, false},
    {32, 16, 8, 8, 8, 0 , 8, 24, 8, V4L2_PIX_FMT_ARGB32, RS_FORMAT_A8R8G8B8, false},
    {32, 0 , 8, 8, 8, 16, 8, 24, 8, 0, RS_FORMAT_A8R8G8B8, true},
    {16, 8 , 4, 4, 4, 0,  4, 0,  0, 0, RS_FORMAT_X4R4G4B4, false},
    {16, 0 , 4, 4, 4, 8,  4, 0,  0, 0, RS_FORMAT_X4R4G4B4, true},
    {16, 8 , 4, 4, 4, 0,  4, 12, 4, 0, RS_FORMAT_A4R4G4B4, false},
    {16, 0 , 4, 4, 4, 8,  4, 12, 4, 0, RS_FORMAT_A4R4G4B4, true},
    {16, 10, 5, 5, 5, 0,  5, 0,  0, 0, RS_FORMAT_X1R5G5B5, false},
    {16, 0,  5, 5, 5, 10, 5, 0,  0, 0, RS_FORMAT_X1R5G5B5, true},
    {16, 10, 5, 5, 5, 0,  5, 15, 1, 0, RS_FORMAT_A1R5G5B5, false},
    {16, 0,  5, 5, 5, 10, 5, 15, 1, 0, RS_FORMAT_A1R5G5B5, true},
    {16, 11, 5, 5, 6, 0,  5, 0,  0, 0, RS_FORMAT_R5G6B5, false},
    {16, 0,  5, 5, 6, 11, 5, 0,  0, 0, RS_FORMAT_R5G6B5, true},
    {16, 0,  0, 0, 0, 0 , 0, 0,  0, V4L2_PIX_FMT_YUYV, RS_FORMAT_YUY2, false},
};

#define NUM_FB_FORMATS (sizeof(etna_fb_formats) / sizeof(etna_fb_formats[0]))

// Align height to 8 to make sure we can use the buffer as target
// for resolve even on GPUs with two pixel pipes
#define ETNA_FB_HEIGHT_ALIGN (8)

/* Get resolve format and swap red/blue format based on report on red/green/blue
 * bit positions from kernel.
 */
static bool etna_fb_get_format(const struct fb_var_screeninfo *fb_var, unsigned *rs_format, bool *swap_rb)
{
    unsigned fmt_idx=0;
    /* linear scan of table to find matching format */
    for(fmt_idx=0; fmt_idx<NUM_FB_FORMATS; ++fmt_idx)
    {
        const struct etna_fb_format_desc *desc = &etna_fb_formats[fmt_idx];
        if(desc->red_offset == fb_var->red.offset &&
            desc->red_length == fb_var->red.length &&
            desc->green_offset == fb_var->green.offset &&
            desc->green_length == fb_var->green.length &&
            desc->blue_offset == fb_var->blue.offset &&
            desc->blue_length == fb_var->blue.length &&
            (desc->alpha_offset == fb_var->transp.offset || desc->alpha_length == 0) &&
            desc->alpha_length == fb_var->transp.length &&
            desc->grayscale == fb_var->grayscale)
        {
            break;
        }
    }
    if(fmt_idx == NUM_FB_FORMATS)
    {
        printf("Unsupported framebuffer format: red_offset=%i red_length=%i green_offset=%i green_length=%i blue_offset=%i blue_length=%i trans_offset=%i transp_length=%i grayscale=%08x\n",
                (int)fb_var->red.offset, (int)fb_var->red.length,
                (int)fb_var->green.offset, (int)fb_var->green.length,
                (int)fb_var->blue.offset, (int)fb_var->blue.length,
                (int)fb_var->transp.offset, (int)fb_var->transp.length,
                (int)fb_var->grayscale);
        return false;
    } else {
        printf("Framebuffer format: %i, flip_rb=%i\n",
                etna_fb_formats[fmt_idx].rs_format,
                etna_fb_formats[fmt_idx].swap_rb);
        *rs_format = etna_fb_formats[fmt_idx].rs_format;
        *swap_rb = etna_fb_formats[fmt_idx].swap_rb;
        return true;
    }
}

/* Open framebuffer and get information */
int fb_open(struct etna_device *conn, int num, struct fb_info **out_p)
{
#ifndef LIBETNAVIV_BO_EXTENSIONS
    printf("Error: fb_open needs libetnaviv extensions\n");
    return -1;
#else
    char devname[256];
    struct fb_info *out = ETNA_CALLOC_STRUCT(fb_info);
    snprintf(devname, 256, FBDEV_DEV, num);

    int fd = open(devname, O_RDWR);
    if (fd == -1) {
        printf("Error: failed to open %s: %s\n",
                devname, strerror(errno));
        return errno;
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &out->fb_var) ||
        ioctl(fd, FBIOGET_FSCREENINFO, &out->fb_fix)) {
            printf("Error: failed to run ioctl on %s: %s\n",
                    devname, strerror(errno));
        close(fd);
        return errno;
    }

    printf("fix smem_start %08x\n", (unsigned)out->fb_fix.smem_start);
    printf("    smem_len %08x\n", (unsigned)out->fb_fix.smem_len);
    printf("    line_length %08x\n", (unsigned)out->fb_fix.line_length);
    printf("\n");
    printf("var x_res %i\n", (unsigned)out->fb_var.xres);
    printf("    y_res %i\n", (unsigned)out->fb_var.yres);
    printf("    x_res_virtual %i\n", (unsigned)out->fb_var.xres_virtual);
    printf("    y_res_virtual %i\n", (unsigned)out->fb_var.yres_virtual);
    printf("    bits_per_pixel %i\n", (unsigned)out->fb_var.bits_per_pixel);
    printf("    red.offset %i\n", (unsigned)out->fb_var.red.offset);
    printf("    red.length %i\n", (unsigned)out->fb_var.red.length);
    printf("    green.offset %i\n", (unsigned)out->fb_var.green.offset);
    printf("    green.length %i\n", (unsigned)out->fb_var.green.length);
    printf("    blue.offset %i\n", (unsigned)out->fb_var.blue.offset);
    printf("    blue.length %i\n", (unsigned)out->fb_var.blue.length);
    printf("    transp.offset %i\n", (unsigned)out->fb_var.transp.offset);
    printf("    transp.length %i\n", (unsigned)out->fb_var.transp.length);
    printf("    grayscale 0x%08x\n", (unsigned)out->fb_var.grayscale);

    out->fd = fd;
    out->stride = out->fb_fix.line_length;
    out->width = out->fb_var.xres;
    out->height = out->fb_var.yres;
    // Align height to make sure we can use the buffer as target
    // for resolve even on GPUs with two pixel pipes
    out->padded_height = etna_align_up(out->fb_var.yres, ETNA_FB_HEIGHT_ALIGN);
    out->buffer_stride = out->stride * out->padded_height;
    out->num_buffers = out->fb_fix.smem_len / out->buffer_stride;

    if(out->num_buffers > ETNA_FB_MAX_BUFFERS)
        out->num_buffers = ETNA_FB_MAX_BUFFERS;
    char *num_buffers_str = getenv("EGL_FBDEV_BUFFERS");
    if(num_buffers_str != NULL)
    {
        int num_buffers_env = atoi(num_buffers_str);
        if(num_buffers_env >= 1 && out->num_buffers > num_buffers_env)
            out->num_buffers = num_buffers_env;
    }
    printf("number of fb buffers: %i\n", out->num_buffers);

    int req_virth = (out->num_buffers * out->padded_height);
    if(out->fb_var.yres_virtual < (unsigned)req_virth)
    {
        printf("required virtual h is %i, current virtual h is %i: requesting change\n",
                req_virth, out->fb_var.yres_virtual);
        out->fb_var.yres_virtual = req_virth;
        if (ioctl(out->fd, FBIOPUT_VSCREENINFO, &out->fb_var))
        {
            printf("Error: failed to run ioctl to change virtual height for buffering: %s. Rendering may fail.\n", strerror(errno));
            return -1;
        }
    }

    out->bo = etna_bo_from_fbdev(conn, fd, 0, out->fb_fix.smem_len);
    if(!out->bo)
    {
        printf("Error: Unable to map framebuffer to GPU\n");
        return -1;
    }

    for(int idx=0; idx<out->num_buffers; ++idx)
    {
        out->buffer[idx].bo = out->bo;
        out->buffer[idx].offset = idx * out->buffer_stride;
        out->buffer[idx].flags = ETNA_RELOC_READ|ETNA_RELOC_WRITE;
    }

    /* determine resolve format */
    if(!etna_fb_get_format(&out->fb_var, (unsigned*)&out->rs_format, &out->swap_rb))
    {
        /* no match */
        printf("Error: No matching framebuffer format\n");
        out->rs_format = -1;
        out->swap_rb = false;
        return -1;
    }

    *out_p = out;
    return 0;
#endif
}

/* Set currently visible buffer id */
int fb_set_buffer(struct fb_info *fb, int buffer)
{
    fb->fb_var.yoffset = buffer * fb->fb_var.yres;
    /* Android uses FBIOPUT_VSCREENINFO for this; however on some hardware this does a
     * reconfiguration of the DC every time it is called which causes flicker and slowness. 
     * On the other hand, FBIOPAN_DISPLAY causes a smooth scroll on some hardware, 
     * according to the Android rationale. Choose the least of both evils.
     */
    if (ioctl(fb->fd, FBIOPAN_DISPLAY, &fb->fb_var))
    {
        printf("Error: failed to run ioctl to pan display: %s\n", strerror(errno));
        return errno;
    }
    return 0;
}

int fb_close(struct fb_info *fb)
{
    etna_bo_del(fb->bo);
    close(fb->fd);
    return 0;
}
