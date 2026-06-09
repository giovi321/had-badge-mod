/* Packet dedup window + rebroadcast gating (the pure backend logic). */
#include "test_util.h"
#include "net/dedup.h"
#include "mesh/packet.h"

void run_dedup(void)
{
    SUITE("dedup/window");
    dedup_t d;
    dedup_init(&d);
    CHECK(dedup_check_and_add(&d, 0x12345678u, 0xAAAA, 100));   /* new */
    CHECK(!dedup_check_and_add(&d, 0x12345678u, 0xAAAA, 101));  /* duplicate */
    CHECK(dedup_check_and_add(&d, 0x12345678u, 0xBBBB, 102));   /* new id */
    CHECK(dedup_check_and_add(&d, 0x99999999u, 0xAAAA, 103));   /* new from */
    CHECK(!dedup_check_and_add(&d, 0x99999999u, 0xAAAA, 104));  /* duplicate */

    SUITE("dedup/ring-overflow");
    dedup_t r;
    dedup_init(&r);
    for (uint32_t i = 0; i < DEDUP_CAP; i++)
        CHECK(dedup_check_and_add(&r, 1, i, i));
    /* table is full; the oldest (id 0) gets overwritten by a new id... */
    CHECK(dedup_check_and_add(&r, 1, 0xF00D, 9999));
    /* ...and id 0 is now forgotten, so it reads as new again */
    CHECK(dedup_check_and_add(&r, 1, 0, 10000));
    /* a recent one is still remembered */
    CHECK(!dedup_check_and_add(&r, 1, DEDUP_CAP - 1, 10001));

    SUITE("dedup/rebroadcast-gate");
    CHECK(mesh_should_rebroadcast(true, MESH_BROADCAST, 3));
    CHECK(!mesh_should_rebroadcast(false, MESH_BROADCAST, 3)); /* disabled */
    CHECK(!mesh_should_rebroadcast(true, 0x12345678u, 3));     /* unicast */
    CHECK(!mesh_should_rebroadcast(true, MESH_BROADCAST, 0));  /* hops exhausted */
}
