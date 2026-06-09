/* Node DB insert/update/LRU + blob roundtrip. */
#include "test_util.h"
#include "core/nodedb.h"
#include <string.h>

void run_nodedb(void)
{
    SUITE("nodedb/upsert");
    nodedb_t db;
    nodedb_init(&db);
    node_record_t *a = nodedb_upsert(&db, 0x1111, 100);
    strcpy(a->long_name, "Alpha");
    CHECK_EQI(db.count, 1);
    node_record_t *a2 = nodedb_upsert(&db, 0x1111, 200);
    CHECK(a == a2);                 /* same record */
    CHECK_EQI(db.count, 1);
    CHECK_EQI(a2->last_heard, 200);
    CHECK(nodedb_get(&db, 0x2222) == NULL);

    SUITE("nodedb/lru-evict");
    nodedb_t full;
    nodedb_init(&full);
    for (int i = 0; i < NODEDB_CAP; i++)
        nodedb_upsert(&full, (uint32_t)(1000 + i), (uint32_t)(i + 1));
    CHECK_EQI(full.count, NODEDB_CAP);
    /* node 1000 has the smallest last_heard (1) -> evicted by a new node */
    nodedb_upsert(&full, 0xDEAD, 99999);
    CHECK_EQI(full.count, NODEDB_CAP);
    CHECK(nodedb_get(&full, 1000) == NULL);
    CHECK(nodedb_get(&full, 0xDEAD) != NULL);
    CHECK(nodedb_get(&full, 1001) != NULL);

    SUITE("nodedb/blob-roundtrip");
    nodedb_t src;
    nodedb_init(&src);
    node_record_t *r = nodedb_upsert(&src, 0xABCD1234, 4242);
    strcpy(r->long_name, "Lecco Badge");
    strcpy(r->short_name, "LBdg");
    r->lat_i = 458566000; r->lon_i = 93976000; r->alt = 214;
    r->snr = 7.25f; r->rssi = -91; r->has_position = true;
    nodedb_upsert(&src, 0x99, 4243);

    uint8_t blob[4096];
    int n = nodedb_serialize(&src, blob, sizeof blob);
    CHECK(n > 0);
    CHECK_EQI((size_t)n, nodedb_blob_size(&src));

    nodedb_t dst;
    CHECK(nodedb_deserialize(&dst, blob, (size_t)n));
    CHECK_EQI(dst.count, 2);
    node_record_t *g = nodedb_get(&dst, 0xABCD1234);
    CHECK(g != NULL);
    CHECK_STR(g->long_name, "Lecco Badge");
    CHECK_STR(g->short_name, "LBdg");
    CHECK_EQI(g->lat_i, 458566000);
    CHECK_EQI(g->alt, 214);
    CHECK_EQI(g->rssi, -91);
    CHECK(g->has_position);
    CHECK(g->snr > 7.24f && g->snr < 7.26f);

    /* a corrupt/foreign blob is rejected */
    uint8_t bad[6] = {0};
    CHECK(!nodedb_deserialize(&dst, bad, sizeof bad));
}
