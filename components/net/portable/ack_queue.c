/* See net/ack_queue.h. */
#include "net/ack_queue.h"

void ackq_init(ackq_t *q)
{
    for (int i = 0; i < ACKQ_CAP; i++) q->e[i].used = false;
}

int ackq_find(const ackq_t *q, uint32_t id)
{
    for (int i = 0; i < ACKQ_CAP; i++)
        if (q->e[i].used && q->e[i].id == id) return i;
    return -1;
}

int ackq_add(ackq_t *q, uint32_t id, uint32_t to, uint32_t now)
{
    int slot = ackq_find(q, id);
    if (slot < 0) {
        for (int i = 0; i < ACKQ_CAP; i++)
            if (!q->e[i].used) { slot = i; break; }
    }
    if (slot < 0) {                         /* full: evict the oldest by last */
        slot = 0;
        for (int i = 1; i < ACKQ_CAP; i++)
            if (q->e[i].last < q->e[slot].last) slot = i;
    }
    q->e[slot].used = true;
    q->e[slot].id = id;
    q->e[slot].to = to;
    q->e[slot].last = now;
    q->e[slot].attempts = 1;
    return slot;
}

int ackq_resolve(ackq_t *q, uint32_t id)
{
    int slot = ackq_find(q, id);
    if (slot >= 0) q->e[slot].used = false;
    return slot;
}

void ackq_tick(ackq_t *q, uint32_t now, uint32_t interval_s, int max_attempts,
               uint32_t *retry, int *n_retry, int retry_cap,
               uint32_t *failed, int *n_failed, int failed_cap)
{
    *n_retry = 0;
    *n_failed = 0;
    for (int i = 0; i < ACKQ_CAP; i++) {
        ackq_entry_t *e = &q->e[i];
        if (!e->used) continue;
        if (now - e->last < interval_s) continue;        /* not due yet */
        if (e->attempts < max_attempts) {
            if (*n_retry < retry_cap) retry[(*n_retry)++] = e->id;
            e->attempts++;
            e->last = now;
        } else {
            if (*n_failed < failed_cap) failed[(*n_failed)++] = e->id;
            e->used = false;
        }
    }
}
