/* SPDX-License-Identifier: MIT */
#ifndef SN_ENCODER_INDEX_H
#define SN_ENCODER_INDEX_H
#include <stdint.h>
/* Caller serializes command/snapshot against the index IRQ. */
typedef struct { uint8_t tag, armed, hit, initialized; int32_t count; } sn_index_state;
static inline void sn_index_command(sn_index_state *s, uint8_t request_high, uint8_t tag) {
    if (!request_high) {
        if (tag == s->tag) { s->armed = 0; s->hit = 0; }
    } else if (!s->hit && !s->armed && (!s->initialized || ((uint8_t)(tag-s->tag) > 0 && (uint8_t)(tag-s->tag) < 128))) {
        s->tag=tag; s->armed=1; s->initialized=1;
    }
}
static inline int sn_index_hit(sn_index_state *s, int32_t count) {
    if (!s->armed || s->hit) return 0;
    s->count=count; s->hit=1; s->armed=0; return 1;
}
#endif
