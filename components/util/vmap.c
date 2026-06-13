/* See util/vmap.h. Decode is always done through the little-endian byte helpers
 * (never an fread straight into a struct) so the host tests and the ESP32 share
 * one code path regardless of native struct padding or endianness. */
#include "util/vmap.h"

#include <string.h>

static uint16_t rd_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int32_t rd_i32(const uint8_t *p)
{
    return (int32_t)rd_u32(p);
}

int vmap_parse_header(const uint8_t *buf, int len, vmap_header_t *out)
{
    if (len < VMAP_HEADER_SIZE) return -1;
    if (rd_u32(buf) != VMAP_MAGIC) return -1;
    out->version = buf[4];
    if (out->version != VMAP_VERSION) return -1;
    out->flags = buf[5];
    /* buf[6..7] reserved */
    out->bbox.min_lat_e7 = rd_i32(buf + 8);
    out->bbox.min_lon_e7 = rd_i32(buf + 12);
    out->bbox.max_lat_e7 = rd_i32(buf + 16);
    out->bbox.max_lon_e7 = rd_i32(buf + 20);
    out->feature_count = rd_u32(buf + 24);
    return VMAP_HEADER_SIZE;
}

int vmap_parse_feature(const uint8_t *buf, int len, vmap_feature_t *out)
{
    if (len < VMAP_FEATURE_PREFIX) return -1;
    out->cls = buf[0];
    /* buf[1] reserved */
    out->point_count = rd_u16(buf + 2);
    out->bbox.min_lat_e7 = rd_i32(buf + 4);
    out->bbox.min_lon_e7 = rd_i32(buf + 8);
    out->bbox.max_lat_e7 = rd_i32(buf + 12);
    out->bbox.max_lon_e7 = rd_i32(buf + 16);
    out->points_off = 0;
    return VMAP_FEATURE_PREFIX;
}

int vmap_open_file(vmap_reader_t *r, FILE *f)
{
    uint8_t hb[VMAP_HEADER_SIZE];
    r->f = f;
    r->index = 0;
    r->next_off = 0;
    if (!f) return -1;
    if (fread(hb, 1, VMAP_HEADER_SIZE, f) != VMAP_HEADER_SIZE) return -1;
    if (vmap_parse_header(hb, VMAP_HEADER_SIZE, &r->hdr) < 0) return -1;
    r->next_off = VMAP_HEADER_SIZE;
    return 0;
}

int vmap_open(vmap_reader_t *r, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { r->f = NULL; return -1; }
    if (vmap_open_file(r, f) < 0) { fclose(f); r->f = NULL; return -1; }
    return 0;
}

int vmap_next(vmap_reader_t *r, vmap_feature_t *out)
{
    uint8_t pb[VMAP_FEATURE_PREFIX];
    if (!r->f) return -1;
    if (r->index >= r->hdr.feature_count) return 0;
    if (fseek(r->f, r->next_off, SEEK_SET) != 0) return -1;
    if (fread(pb, 1, VMAP_FEATURE_PREFIX, r->f) != VMAP_FEATURE_PREFIX) return -1;
    if (vmap_parse_feature(pb, VMAP_FEATURE_PREFIX, out) < 0) return -1;
    out->points_off = r->next_off + VMAP_FEATURE_PREFIX;
    r->next_off = out->points_off + (long)out->point_count * 8;
    r->index++;
    return 1;
}

int vmap_read_points(vmap_reader_t *r, const vmap_feature_t *ft, int32_t *pts, int cap)
{
    int pc = (int)ft->point_count;
    if (!r->f || pc < 0 || 2 * pc > cap) return -1;
    if (fseek(r->f, ft->points_off, SEEK_SET) != 0) return -1;
    for (int i = 0; i < pc; i++) {
        uint8_t b[8];
        if (fread(b, 1, 8, r->f) != 8) return -1;
        pts[2 * i]     = rd_i32(b);
        pts[2 * i + 1] = rd_i32(b + 4);
    }
    return pc;
}

int vmap_skip_points(vmap_reader_t *r, const vmap_feature_t *ft)
{
    if (!r->f) return -1;
    return fseek(r->f, ft->points_off + (long)ft->point_count * 8, SEEK_SET) == 0 ? 0 : -1;
}

void vmap_close(vmap_reader_t *r)
{
    if (r->f) { fclose(r->f); r->f = NULL; }
}
