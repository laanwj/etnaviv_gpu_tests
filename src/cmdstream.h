#ifndef H_CMDSTREAM
#define H_CMDSTREAM

#include <etnaviv_drmif.h>

#include "hw/cmdstream.xml.h"
#include "hw/common.xml.h"
#include "hw/state.xml.h"

static inline void etna_emit_load_state(struct etna_cmd_stream *stream,
        const uint16_t offset, const uint16_t count, const int fixp)
{
    uint32_t v;

    v = VIV_FE_LOAD_STATE_HEADER_OP_LOAD_STATE |
        (fixp?VIV_FE_LOAD_STATE_HEADER_FIXP:0) |
        VIV_FE_LOAD_STATE_HEADER_OFFSET(offset) |
        (VIV_FE_LOAD_STATE_HEADER_COUNT(count) &
         VIV_FE_LOAD_STATE_HEADER_COUNT__MASK);

    etna_cmd_stream_emit(stream, v);
}

static inline void etna_set_state(struct etna_cmd_stream *stream, uint32_t address, uint32_t value)
{
    etna_cmd_stream_reserve(stream, 2);
    etna_emit_load_state(stream, address >> 2, 1, 0);
    etna_cmd_stream_emit(stream, value);
}

static inline void etna_set_state_fixp(struct etna_cmd_stream *stream, uint32_t address, uint32_t value)
{
    etna_cmd_stream_reserve(stream, 2);
    etna_emit_load_state(stream, address >> 2, 1, 1);
    etna_cmd_stream_emit(stream, value);
}

static inline void etna_set_state_reloc(struct etna_cmd_stream *stream, uint32_t address, const struct etna_reloc *reloc)
{
    etna_cmd_stream_reserve(stream, 2);
    etna_emit_load_state(stream, address >> 2, 1, 0);
    etna_cmd_stream_reloc(stream, reloc);
}

static inline void
etna_set_state_multi(struct etna_cmd_stream *stream, uint32_t base,
                     uint32_t num, const uint32_t *values)
{
   if (num == 0)
      return;

   etna_cmd_stream_reserve(stream, 1 + num + 1); /* 1 extra for potential alignment */
   etna_emit_load_state(stream, base >> 2, num, 0);

   for (uint32_t i = 0; i < num; i++)
      etna_cmd_stream_emit(stream, values[i]);

   /* add potential padding */
   if ((num % 2) == 0)
      etna_cmd_stream_emit(stream, 0);
}

/* reloc_flags must be combo of ETNA_RELOC_WRITE, ETNA_RELOC_READ */
static inline void etna_set_state_from_bo(struct etna_cmd_stream *stream,
        uint32_t address, struct etna_bo *bo, uint32_t reloc_flags)
{
    etna_cmd_stream_reserve(stream, 2);
    etna_emit_load_state(stream, address >> 2, 1, 0);

    etna_cmd_stream_reloc(stream, &(struct etna_reloc){
        .bo = bo,
        .flags = reloc_flags,
        .offset = 0,
    });
}

static inline void etna_stall(struct etna_cmd_stream *stream, uint32_t from, uint32_t to)
{
   etna_cmd_stream_reserve(stream, 4);

   etna_emit_load_state(stream, VIVS_GL_SEMAPHORE_TOKEN >> 2, 1, 0);
   etna_cmd_stream_emit(stream, VIVS_GL_SEMAPHORE_TOKEN_FROM(from) | VIVS_GL_SEMAPHORE_TOKEN_TO(to));

   if (from == SYNC_RECIPIENT_FE) {
      /* if the frontend is to be stalled, queue a STALL frontend command */
      etna_cmd_stream_emit(stream, VIV_FE_STALL_HEADER_OP_STALL);
      etna_cmd_stream_emit(stream, VIV_FE_STALL_TOKEN_FROM(from) | VIV_FE_STALL_TOKEN_TO(to));
   } else {
      /* otherwise, load the STALL token state */
      etna_emit_load_state(stream, VIVS_GL_STALL_TOKEN >> 2, 1, 0);
      etna_cmd_stream_emit(stream, VIVS_GL_STALL_TOKEN_FROM(from) | VIVS_GL_STALL_TOKEN_TO(to));
   }
}

#endif
