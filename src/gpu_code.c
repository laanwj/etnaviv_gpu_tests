#include "gpu_code.h"

#include <string.h>

struct gpu_code *gpu_code_new(const uint32_t *codein, size_t size_uints)
{
    struct gpu_code *code = CALLOC_STRUCT(gpu_code);
    uint32_t *code_ptr = malloc(size_uints * 4);
    code->size = size_uints;
    memcpy(code_ptr, codein, size_uints * 4);
    code->code = code_ptr;
    return code;
}

void gpu_code_destroy(struct gpu_code *code)
{
    if (!code) {
        return;
    }
    free((void*)code->code);
    gpu_code_destroy_bo(code);
    free(code);
}

void gpu_code_alloc_bo(struct gpu_code *code, struct etna_device *dev)
{
    code->bo = etna_bo_new(dev, code->size*4, DRM_ETNA_GEM_CACHE_UNCACHED);
    memcpy(etna_bo_map(code->bo), code->code, code->size*4);
}

void gpu_code_destroy_bo(struct gpu_code *code)
{
    if (!code->bo)
        return;
    etna_bo_del(code->bo);
    code->bo = NULL;
}
