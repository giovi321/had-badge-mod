/* Mesh node database: a fixed-capacity table of heard nodes, LRU-evicted by
 * last_heard, serialisable to a flat blob for NVS persistence. LVGL/IDF-free. */
#ifndef CORE_NODEDB_H
#define CORE_NODEDB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef NODEDB_CAP
#define NODEDB_CAP 64
#endif

typedef struct {
    uint32_t num;            /* node number (low 32 bits of node id) */
    char long_name[48];
    char short_name[16];
    int32_t lat_i, lon_i;    /* degrees * 1e7 */
    int32_t alt;             /* metres */
    float snr;
    int16_t rssi;
    uint32_t last_heard;     /* unix seconds */
    bool has_position;
    uint8_t battery;         /* percent, from telemetry (0 if unknown) */
    float voltage;
    bool has_telemetry;
} node_record_t;

typedef struct {
    node_record_t nodes[NODEDB_CAP];
    int count;
} nodedb_t;

void nodedb_init(nodedb_t *db);

/* Find a node by number, or NULL. */
node_record_t *nodedb_get(nodedb_t *db, uint32_t num);

/* Return the record for `num`, creating it if absent. If the table is full a
 * new node evicts the least-recently-heard one. Stamps last_heard = now.
 * Never returns NULL. */
node_record_t *nodedb_upsert(nodedb_t *db, uint32_t num, uint32_t now);

/* Serialise to a self-describing blob. Returns bytes written, or -1 if cap is
 * too small. nodedb_blob_size() gives the exact size needed. */
size_t nodedb_blob_size(const nodedb_t *db);
int nodedb_serialize(const nodedb_t *db, uint8_t *buf, size_t cap);
bool nodedb_deserialize(nodedb_t *db, const uint8_t *buf, size_t len);

#endif /* CORE_NODEDB_H */
