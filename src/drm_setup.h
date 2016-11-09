#ifndef H_DRM_SETUP
#define H_DRM_SETUP

#include <etnaviv_drmif.h>

struct drm_test_info
{
    int fd;
    struct etna_device *dev;
    struct etna_gpu *gpu;
    struct etna_pipe *pipe;
    struct etna_cmd_stream *stream;
};

struct drm_test_info *drm_test_setup(int argc, char **argv);
void drm_test_teardown(struct drm_test_info *info);

#endif
