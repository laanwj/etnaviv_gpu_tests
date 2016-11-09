#ifndef H_CMDSTREAM
#define H_CMDSTREAM

#include <etnaviv_drmif.h>

#include "cmdstream.xml.h"

static inline void etna_emit_load_state(struct etna_cmd_stream *stream,
        const uint16_t offset, const uint16_t count)
{
    uint32_t v;

    v =     (VIV_FE_LOAD_STATE_HEADER_OP_LOAD_STATE | VIV_FE_LOAD_STATE_HEADER_OFFSET(offset) |
            (VIV_FE_LOAD_STATE_HEADER_COUNT(count) & VIV_FE_LOAD_STATE_HEADER_COUNT__MASK));

    etna_cmd_stream_emit(stream, v);
}

static inline void etna_set_state(struct etna_cmd_stream *stream, uint32_t address, uint32_t value)
{
    etna_cmd_stream_reserve(stream, 2);
    etna_emit_load_state(stream, address >> 2, 1);
    etna_cmd_stream_emit(stream, value);
}

static inline void etna_set_state_from_bo(struct etna_cmd_stream *stream,
        uint32_t address, struct etna_bo *bo)
{
    etna_cmd_stream_reserve(stream, 2);
    etna_emit_load_state(stream, address >> 2, 1);

    etna_cmd_stream_reloc(stream, &(struct etna_reloc){
        .bo = bo,
        .flags = ETNA_RELOC_WRITE,
        .offset = 0,
    });
}

#endif
