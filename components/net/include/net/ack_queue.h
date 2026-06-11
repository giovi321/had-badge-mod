/* Retry/ack bookkeeping for outgoing unicast messages. Portable and host-tested
 * (no IDF): tracks which packet ids are awaiting a delivery ack, decides when to
 * retransmit, and gives up after a few attempts. The backend keeps the actual
 * frame bytes in a parallel slot and re-sends on the ids this returns. */
#ifndef NET_ACK_QUEUE_H
#define NET_ACK_QUEUE_H

#include <stdint.h>
#include <stdbool.h>

#ifndef ACKQ_CAP
#define ACKQ_CAP 8
#endif

typedef struct {
    uint32_t id;        /* packet id awaiting a delivery ack */
    uint32_t to;        /* recipient node */
    uint32_t last;      /* time of the last (re)transmission */
    uint16_t attempts;  /* transmissions so far (1 = the original send) */
    bool used;
} ackq_entry_t;

typedef struct { ackq_entry_t e[ACKQ_CAP]; } ackq_t;

void ackq_init(ackq_t *q);

/* Track a freshly-sent unicast (attempts = 1). Reuses the slot for the same id;
 * evicts the least-recently-touched entry when full. Returns the slot index. */
int ackq_add(ackq_t *q, uint32_t id, uint32_t to, uint32_t now);

/* Slot of a tracked id, or -1. Used to fetch the frame to retransmit. */
int ackq_find(const ackq_t *q, uint32_t id);

/* Mark id delivered and free its slot. Returns the slot, or -1 if not tracked. */
int ackq_resolve(ackq_t *q, uint32_t id);

/* Advance time. For every entry whose last (re)transmit is at least interval_s
 * old: if attempts remain (< max_attempts) it is due for a resend -- its id is
 * appended to retry[] and its attempts/last are bumped; if max_attempts is
 * reached it has failed -- its id is appended to failed[] and the slot is freed.
 * n_retry / n_failed are set to the counts (bounded by the caps). */
void ackq_tick(ackq_t *q, uint32_t now, uint32_t interval_s, int max_attempts,
               uint32_t *retry, int *n_retry, int retry_cap,
               uint32_t *failed, int *n_failed, int failed_cap);

#endif /* NET_ACK_QUEUE_H */
