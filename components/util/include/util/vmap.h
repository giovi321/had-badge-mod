/* Reader for the offline vector-map (.vmap) file the radar overlay draws.
 *
 * The format is a flat list of polyline features, each tagged ROAD or WATER and
 * prefixed by its own lat/lon bounding box so the device can cull out-of-view
 * features without reading their points. Coordinates are fixed-point 1e-7
 * degrees (int32), little-endian throughout. Produced on a PC by
 * tools/osm2vmap.py and uploaded to /spiffs/map.vmap.
 *
 * Two layers: pure byte decoders (vmap_parse_*) that are stdio-free and easy to
 * host-test from a buffer, and a streaming FILE* reader built on them that keeps
 * only one feature's points in memory at a time. Portable and host-tested. */
#ifndef UTIL_VMAP_H
#define UTIL_VMAP_H

#include <stdint.h>
#include <stdio.h>

#include "util/map_proj.h"   /* map_bbox_e7_t */

#define VMAP_MAGIC        0x50414D56u   /* 'V''M''A''P' little-endian */
#define VMAP_VERSION      1
#define VMAP_CLASS_ROAD   1
#define VMAP_CLASS_WATER  2
#define VMAP_MAX_POINTS   256           /* per feature; the tool splits longer ways */
#define VMAP_E7           1.0e7

#define VMAP_HEADER_SIZE      28
#define VMAP_FEATURE_PREFIX   20

/* Canonical on-device location the web upload writes and the radar reads. */
#define VMAP_DEFAULT_PATH     "/spiffs/map.vmap"

typedef struct {
    uint8_t       version;
    uint8_t       flags;
    map_bbox_e7_t bbox;
    uint32_t      feature_count;
} vmap_header_t;

typedef struct {
    uint8_t       cls;          /* VMAP_CLASS_* (unknown values are skipped) */
    uint16_t      point_count;
    map_bbox_e7_t bbox;
    long          points_off;   /* file offset of this feature's first point */
} vmap_feature_t;

typedef struct {
    FILE         *f;
    vmap_header_t hdr;
    uint32_t      index;        /* features returned so far */
    long          next_off;     /* file offset of the next feature prefix */
} vmap_reader_t;

/* Pure decoders over a raw little-endian buffer. Return the number of bytes the
 * record occupies (VMAP_HEADER_SIZE / VMAP_FEATURE_PREFIX) or <0 on a too-short
 * or invalid buffer. vmap_parse_feature leaves out->points_off = 0. */
int vmap_parse_header(const uint8_t *buf, int len, vmap_header_t *out);
int vmap_parse_feature(const uint8_t *buf, int len, vmap_feature_t *out);

/* Open a .vmap file (or wrap an already-open binary stream) and validate the
 * header. Return 0 on success, <0 on error. On success the reader is positioned
 * at the first feature. vmap_close() closes the underlying FILE in both cases. */
int  vmap_open(vmap_reader_t *r, const char *path);
int  vmap_open_file(vmap_reader_t *r, FILE *f);

/* Read the next feature prefix into `out`. Returns 1 (feature), 0 (end), or <0
 * (error). After a 1, call exactly one of vmap_read_points / vmap_skip_points
 * before the next vmap_next. */
int  vmap_next(vmap_reader_t *r, vmap_feature_t *out);

/* Read ft's points as interleaved (lat_e7, lon_e7) int32 pairs into `pts`.
 * `cap` is the capacity of `pts` in int32 slots (need 2 * point_count). Returns
 * the point count read (== ft->point_count) or <0 on error. */
int  vmap_read_points(vmap_reader_t *r, const vmap_feature_t *ft, int32_t *pts, int cap);

/* Advance past ft's points without reading them. Returns 0 or <0 on error. */
int  vmap_skip_points(vmap_reader_t *r, const vmap_feature_t *ft);

void vmap_close(vmap_reader_t *r);

#endif /* UTIL_VMAP_H */
