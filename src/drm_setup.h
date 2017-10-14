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

/** These are not defined by the base drm interface,
 * but useful for tracking.
 */
#ifndef DRM_ETNA_GEM_TYPE_MASK
#define DRM_ETNA_GEM_TYPE_GEN        0x00000000 /* General */
#define DRM_ETNA_GEM_TYPE_IDX        0x00000001 /* Index buffer */
#define DRM_ETNA_GEM_TYPE_VTX        0x00000002 /* Vertex buffer */
#define DRM_ETNA_GEM_TYPE_TEX        0x00000003 /* Texture */
#define DRM_ETNA_GEM_TYPE_RT         0x00000004 /* Color render target */
#define DRM_ETNA_GEM_TYPE_ZS         0x00000005 /* Depth stencil target */
#define DRM_ETNA_GEM_TYPE_HZ         0x00000006 /* Hierarchical depth render target */
#define DRM_ETNA_GEM_TYPE_BMP        0x00000007 /* Bitmap */
#define DRM_ETNA_GEM_TYPE_TS         0x00000008 /* Tile status cache */
#define DRM_ETNA_GEM_TYPE_TXD        0x00000009 /* Texture descriptor */
#define DRM_ETNA_GEM_TYPE_IC         0x0000000A /* Instruction cache (Shader code) */
#define DRM_ETNA_GEM_TYPE_CMD        0x0000000B /* Command buffer */
#define DRM_ETNA_GEM_TYPE_MASK       0x0000000F
#endif

extern struct drm_test_info *drm_test_setup(int argc, char **argv);

extern void drm_test_teardown(struct drm_test_info *info);

extern enum hardware_type drm_cl_get_hardware_type(struct drm_test_info *info);

#endif
