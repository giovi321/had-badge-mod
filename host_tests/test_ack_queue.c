/* Retry/ack queue (net/ack_queue): add/resolve, retry timing, give-up, eviction. */
#include "test_util.h"
#include "net/ack_queue.h"

void run_ack_queue(void)
{
    ackq_t q;
    uint32_t retry[ACKQ_CAP], failed[ACKQ_CAP];
    int nr, nf;

    SUITE("ackq/add-resolve");
    ackq_init(&q);
    int s = ackq_add(&q, 100, 7, 1000);
    CHECK(s >= 0);
    CHECK_EQI(ackq_find(&q, 100), s);
    CHECK_EQI(ackq_resolve(&q, 100), s);
    CHECK_EQI(ackq_resolve(&q, 100), -1);   /* gone after resolve */

    SUITE("ackq/no-retry-before-interval");
    ackq_init(&q);
    ackq_add(&q, 200, 7, 1000);
    ackq_tick(&q, 1005, 20, 3, retry, &nr, ACKQ_CAP, failed, &nf, ACKQ_CAP);   /* 5s < 20s */
    CHECK_EQI(nr, 0);
    CHECK_EQI(nf, 0);

    SUITE("ackq/retry-then-fail");
    ackq_init(&q);
    ackq_add(&q, 300, 7, 1000);             /* attempt 1 */
    ackq_tick(&q, 1025, 20, 3, retry, &nr, ACKQ_CAP, failed, &nf, ACKQ_CAP);   /* -> attempt 2 */
    CHECK_EQI(nr, 1);
    CHECK_EQI(retry[0], 300);
    CHECK_EQI(nf, 0);
    ackq_tick(&q, 1050, 20, 3, retry, &nr, ACKQ_CAP, failed, &nf, ACKQ_CAP);   /* -> attempt 3 */
    CHECK_EQI(nr, 1);
    CHECK_EQI(nf, 0);
    ackq_tick(&q, 1075, 20, 3, retry, &nr, ACKQ_CAP, failed, &nf, ACKQ_CAP);   /* exhausted -> fail */
    CHECK_EQI(nr, 0);
    CHECK_EQI(nf, 1);
    CHECK_EQI(failed[0], 300);
    CHECK_EQI(ackq_resolve(&q, 300), -1);   /* freed on failure */

    SUITE("ackq/ack-stops-retry");
    ackq_init(&q);
    ackq_add(&q, 400, 7, 1000);
    CHECK(ackq_resolve(&q, 400) >= 0);
    ackq_tick(&q, 2000, 20, 3, retry, &nr, ACKQ_CAP, failed, &nf, ACKQ_CAP);
    CHECK_EQI(nr, 0);
    CHECK_EQI(nf, 0);                        /* nothing tracked anymore */

    SUITE("ackq/evict-when-full");
    ackq_init(&q);
    for (int i = 0; i < ACKQ_CAP; i++) ackq_add(&q, 500 + i, 7, 1000 + i);   /* oldest = id 500 */
    ackq_add(&q, 999, 7, 2000);             /* evicts the oldest */
    CHECK_EQI(ackq_find(&q, 500), -1);
    CHECK(ackq_find(&q, 999) >= 0);
}
