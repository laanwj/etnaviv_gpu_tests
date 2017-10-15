#include "drm_setup.h"
#include "memutil.h"

#include <common.xml.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

/** Find first GPU with a 3D pipe */
struct etna_gpu *find_suitable_gpu(struct etna_device *dev)
{
    int core_id = 0;
    while (1) {
        struct etna_gpu *gpu = etna_gpu_new(dev, core_id);
        if (!gpu) { /* No more cores, stop */
            return NULL;
        }
        uint64_t features = 0;
        int rv = etna_gpu_get_param(gpu, ETNA_GPU_FEATURES_0, &features);
        if (rv >= 0 && (features & chipFeatures_PIPE_3D)) {
            return gpu;
        }
        etna_gpu_del(gpu);
    }
}

struct drm_test_info *drm_test_setup(int argc, char **argv)
{
    struct drm_test_info *info;
    drmVersionPtr version;

    if (argc < 2) {
        printf("Usage: %s /dev/dri/...\n", argv[0]);
        return NULL;
    }
    info = CALLOC_STRUCT(drm_test_info);
    info->fd = open(argv[1], O_RDWR);
    if (info->fd < 0) {
        fprintf(stdout, "Unable to open %s\n", argv[1]);
        goto out;
    }

    version = drmGetVersion(info->fd);
    if (version) {
        printf("Version: %d.%d.%d\n", version->version_major,
               version->version_minor, version->version_patchlevel);
        printf("  Name: %s\n", version->name);
        printf("  Date: %s\n", version->date);
        printf("  Description: %s\n", version->desc);
        drmFreeVersion(version);
    }

    info->dev = etna_device_new(info->fd);
    if (!info->dev) {
        fprintf(stdout, "Unable to create device\n");
        goto out;
    }

    info->gpu = find_suitable_gpu(info->dev);
    if (!info->gpu) {
        fprintf(stdout, "Unable to find gpu with 3D pipe\n");
        goto out_device;
    }

    info->pipe = etna_pipe_new(info->gpu, ETNA_PIPE_3D);
    if (!info->pipe) {
        fprintf(stdout, "Unable to create pipe\n");
        goto out_gpu;
    }

    info->stream = etna_cmd_stream_new(info->pipe, 0x3000, NULL, NULL);
    if (!info->stream) {
        fprintf(stdout, "Unable to create command stream\n");
        goto out_pipe;
    }
    return info;

out_pipe:
    etna_pipe_del(info->pipe);

out_gpu:
    etna_gpu_del(info->gpu);

out_device:
    etna_device_del(info->dev);

out:
    close(info->fd);
    free(info);
    return NULL;
}

void drm_test_teardown(struct drm_test_info *info)
{
    etna_cmd_stream_del(info->stream);
    etna_pipe_del(info->pipe);
    etna_gpu_del(info->gpu);
    etna_device_del(info->dev);
    close(info->fd);
    free(info);
}

enum hardware_type drm_cl_get_hardware_type(struct drm_test_info *info)
{
    uint64_t val;
    enum hardware_type hwt = HWT_OTHER;
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
error:
    return hwt;
}
