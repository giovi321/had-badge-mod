/* See core/nodedb.h. */
#include "core/nodedb.h"
#include <string.h>

#define NODEDB_MAGIC 0x3342444Eu /* "NDB3" */
#define REC_BYTES (4 + 48 + 16 + 4 + 4 + 4 + 4 + 2 + 4 + 1 + 1 + 4 + 1 \
                   + 4 + 4 + 1 + 4 + 4 + 4 + 1) /* = 119 */

void nodedb_init(nodedb_t *db) { memset(db, 0, sizeof(*db)); }

node_record_t *nodedb_get(nodedb_t *db, uint32_t num)
{
    for (int i = 0; i < db->count; i++)
        if (db->nodes[i].num == num) return &db->nodes[i];
    return NULL;
}

node_record_t *nodedb_upsert(nodedb_t *db, uint32_t num, uint32_t now)
{
    node_record_t *r = nodedb_get(db, num);
    if (r) { r->last_heard = now; return r; }

    int idx;
    if (db->count < NODEDB_CAP) {
        idx = db->count++;
    } else {
        idx = 0; /* evict least-recently-heard */
        for (int i = 1; i < db->count; i++)
            if (db->nodes[i].last_heard < db->nodes[idx].last_heard) idx = i;
    }
    memset(&db->nodes[idx], 0, sizeof(node_record_t));
    db->nodes[idx].num = num;
    db->nodes[idx].last_heard = now;
    return &db->nodes[idx];
}

/* --- serialisation (explicit little-endian) ---------------------------- */
static uint8_t *put_u16(uint8_t *p, uint16_t v) { *p++ = (uint8_t)v; *p++ = (uint8_t)(v >> 8); return p; }
static uint8_t *put_u32(uint8_t *p, uint32_t v)
{
    *p++ = (uint8_t)v; *p++ = (uint8_t)(v >> 8);
    *p++ = (uint8_t)(v >> 16); *p++ = (uint8_t)(v >> 24);
    return p;
}
static const uint8_t *get_u16(const uint8_t *p, uint16_t *v) { *v = (uint16_t)(p[0] | (p[1] << 8)); return p + 2; }
static const uint8_t *get_u32(const uint8_t *p, uint32_t *v)
{
    *v = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    return p + 4;
}

size_t nodedb_blob_size(const nodedb_t *db) { return 6 + (size_t)db->count * REC_BYTES; }

int nodedb_serialize(const nodedb_t *db, uint8_t *buf, size_t cap)
{
    size_t need = nodedb_blob_size(db);
    if (cap < need) return -1;
    uint8_t *p = buf;
    p = put_u32(p, NODEDB_MAGIC);
    p = put_u16(p, (uint16_t)db->count);
    for (int i = 0; i < db->count; i++) {
        const node_record_t *r = &db->nodes[i];
        p = put_u32(p, r->num);
        memcpy(p, r->long_name, 48);  p += 48;
        memcpy(p, r->short_name, 16); p += 16;
        p = put_u32(p, (uint32_t)r->lat_i);
        p = put_u32(p, (uint32_t)r->lon_i);
        p = put_u32(p, (uint32_t)r->alt);
        memcpy(p, &r->snr, 4); p += 4;
        p = put_u16(p, (uint16_t)r->rssi);
        p = put_u32(p, r->last_heard);
        *p++ = r->has_position ? 1 : 0;
        *p++ = r->battery;
        memcpy(p, &r->voltage, 4); p += 4;
        *p++ = r->has_telemetry ? 1 : 0;
        memcpy(p, &r->speed, 4); p += 4;
        memcpy(p, &r->course, 4); p += 4;
        *p++ = r->has_motion ? 1 : 0;
        memcpy(p, &r->temperature, 4); p += 4;
        memcpy(p, &r->humidity, 4); p += 4;
        memcpy(p, &r->pressure, 4); p += 4;
        *p++ = r->has_env ? 1 : 0;
    }
    return (int)need;
}

bool nodedb_deserialize(nodedb_t *db, const uint8_t *buf, size_t len)
{
    nodedb_init(db);
    if (len < 6) return false;
    const uint8_t *p = buf;
    uint32_t magic; uint16_t count;
    p = get_u32(p, &magic);
    p = get_u16(p, &count);
    if (magic != NODEDB_MAGIC) return false;
    if ((size_t)6 + (size_t)count * REC_BYTES > len) return false;
    if (count > NODEDB_CAP) count = NODEDB_CAP;
    for (int i = 0; i < count; i++) {
        node_record_t *r = &db->nodes[i];
        memset(r, 0, sizeof(*r));
        uint32_t u;
        p = get_u32(p, &r->num);
        memcpy(r->long_name, p, 48);  p += 48; r->long_name[47] = 0;
        memcpy(r->short_name, p, 16); p += 16; r->short_name[15] = 0;
        p = get_u32(p, &u); r->lat_i = (int32_t)u;
        p = get_u32(p, &u); r->lon_i = (int32_t)u;
        p = get_u32(p, &u); r->alt = (int32_t)u;
        memcpy(&r->snr, p, 4); p += 4;
        uint16_t r16; p = get_u16(p, &r16); r->rssi = (int16_t)r16;
        p = get_u32(p, &r->last_heard);
        r->has_position = (*p++ != 0);
        r->battery = *p++;
        memcpy(&r->voltage, p, 4); p += 4;
        r->has_telemetry = (*p++ != 0);
        memcpy(&r->speed, p, 4); p += 4;
        memcpy(&r->course, p, 4); p += 4;
        r->has_motion = (*p++ != 0);
        memcpy(&r->temperature, p, 4); p += 4;
        memcpy(&r->humidity, p, 4); p += 4;
        memcpy(&r->pressure, p, 4); p += 4;
        r->has_env = (*p++ != 0);
    }
    db->count = count;
    return true;
}
