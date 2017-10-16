#ifndef H_COLOR
#define H_COLOR

#include "etna_util.h"

static inline uint32_t hsv_argb(float h, float s, float v, float a)
{
    float hr = etna_clampf(fabs(h * 6.0f - 3.0f) - 1.0f);
    float hg = etna_clampf(2.0f - fabs(h * 6.0f - 2.0f));
    float hb = etna_clampf(2.0f - fabs(h * 6.0f - 4.0f));
    float r = ((hr - 1.0f) * s + 1.0f) * v;
    float g = ((hg - 1.0f) * s + 1.0f) * v;
    float b = ((hb - 1.0f) * s + 1.0f) * v;
    return (etna_cfloat_to_uint8(a) << 24) |
        (etna_cfloat_to_uint8(r) << 16) |
        (etna_cfloat_to_uint8(g) << 8) |
        (etna_cfloat_to_uint8(b) << 0);
}

static inline uint32_t rgba_argb(float r, float g, float b, float a)
{
    return (etna_cfloat_to_uint8(a) << 24) |
        (etna_cfloat_to_uint8(r) << 16) |
        (etna_cfloat_to_uint8(g) << 8) |
        (etna_cfloat_to_uint8(b) << 0);
}

#endif
