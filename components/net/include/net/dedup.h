/* Packet de-duplication window + rebroadcast gating (the pure logic from
 * backend_meshtastic.py _handle_frame). LVGL/IDF-free, host-tested. */
#ifndef NET_DEDUP_H
#define NET_DEDUP_H

#include <stdint.h>
#include <stdbool.h>

#ifndef DEDUP_CAP
#define DEDUP_CAP 256
#endif

typedef struct {
    struct { uint32_t from; uint32_t id; uint32_t when; } slots[DEDUP_CAP];
    int count;   /* number of valid slots (<= DEDUP_CAP) */
    int head;    /* next slot to overwrite once full (ring) */
} dedup_t;

void dedup_init(dedup_t *d);

/* If (from,id) was already seen, return false (and do nothing). Otherwise record
 * it (overwriting the oldest entry when full) and return true ("new"). */
bool dedup_check_and_add(dedup_t *d, uint32_t from, uint32_t id, uint32_t now);

/* Should this received broadcast be rebroadcast? Mirrors the backend rule:
 * rebroadcast enabled AND to == 0xFFFFFFFF AND hop_limit > 0. */
bool mesh_should_rebroadcast(bool rebroadcast_enabled, uint32_t to, uint8_t hop_limit);

#endif /* NET_DEDUP_H */
