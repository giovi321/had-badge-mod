/* See net/dedup.h. */
#include "net/dedup.h"
#include "mesh/packet.h"  /* MESH_BROADCAST */

void dedup_init(dedup_t *d)
{
    d->count = 0;
    d->head = 0;
}

bool dedup_check_and_add(dedup_t *d, uint32_t from, uint32_t id, uint32_t now)
{
    for (int i = 0; i < d->count; i++)
        if (d->slots[i].from == from && d->slots[i].id == id)
            return false; /* duplicate */

    int idx;
    if (d->count < DEDUP_CAP) {
        idx = d->count++;
    } else {
        idx = d->head;
        d->head = (d->head + 1) % DEDUP_CAP; /* overwrite oldest */
    }
    d->slots[idx].from = from;
    d->slots[idx].id = id;
    d->slots[idx].when = now;
    return true;
}

bool mesh_should_rebroadcast(bool rebroadcast_enabled, uint32_t to, uint8_t hop_limit)
{
    return rebroadcast_enabled && to == MESH_BROADCAST && hop_limit > 0;
}
