#ifndef H_DRM_SETUP
#define H_DRM_SETUP

#include <etnaviv_drmif.h>

enum hardware_type {
    HWT_OTHER = 0,
    HWT_GC2000 = 1,
    HWT_GC3000 = 2,
    HWT_ALL = 3,
};

struct drm_test_info
{
    int fd;
    struct etna_device *dev;
    struct etna_gpu *gpu;
    struct etna_pipe *pipe;
    struct etna_cmd_stream *stream;
};

extern struct drm_test_info *drm_test_setup(int argc, char **argv);

extern void drm_test_teardown(struct drm_test_info *info);

extern enum hardware_type drm_cl_get_hardware_type(struct drm_test_info *info);

#endif
