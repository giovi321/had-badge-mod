/* Vector-map overlay checks: the projection/clip helpers (util/map_proj) and
 * the .vmap reader (util/vmap). The projection is verified to agree with the
 * radar blip projection so map lines and node blips line up on the scope. */
#include "test_util.h"
#include "util/map_proj.h"
#include "util/vmap.h"
#include "util/radar_proj.h"

#include <stdio.h>

/* Little-endian buffer writers for assembling a synthetic .vmap file. */
static int put_u16(uint8_t *b, int o, uint16_t v)
{
    b[o] = (uint8_t)(v & 0xff); b[o + 1] = (uint8_t)(v >> 8); return o + 2;
}
static int put_u32(uint8_t *b, int o, uint32_t v)
{
    b[o] = (uint8_t)(v & 0xff); b[o + 1] = (uint8_t)((v >> 8) & 0xff);
    b[o + 2] = (uint8_t)((v >> 16) & 0xff); b[o + 3] = (uint8_t)((v >> 24) & 0xff);
    return o + 4;
}
static int put_i32(uint8_t *b, int o, int32_t v) { return put_u32(b, o, (uint32_t)v); }

void run_map(void)
{
    const float CX = 46, CY = 46, R = 42;

    /* ---- projection: alignment with radar_project ---------------------- */
    SUITE("map/align-radar");
    {
        /* Several nodes around you, well inside a 1 km scope. map_project must
         * land within half a pixel of where radar_project puts the same node. */
        const double clat = 47.0, clon = 8.0;
        const double dl[][2] = {
            { 0.005,  0.000}, {0.000,  0.005}, {-0.004, 0.000},
            {0.000, -0.006}, {0.003,  0.004}, {-0.002, -0.003},
        };
        const double ups[] = {0.0, 90.0, 215.0};
        for (int u = 0; u < 3; u++) {
            for (int i = 0; i < 6; i++) {
                double plat = clat + dl[i][0], plon = clon + dl[i][1];
                radar_blip_t rb = radar_project(clat, clon, plat, plon, ups[u],
                                                1000.0, CX, CY, R);
                float mx, my;
                map_project(clat, clon, plat, plon, ups[u], 1000.0, CX, CY, R, &mx, &my);
                CHECK_NEAR(mx, rb.x, 0.5);
                CHECK_NEAR(my, rb.y, 0.5);
            }
        }
    }

    SUITE("map/center");
    {
        float x, y;
        map_project(1.23, 4.56, 1.23, 4.56, 0.0, 1000.0, CX, CY, R, &x, &y);
        CHECK_NEAR(x, CX, 0.01);
        CHECK_NEAR(y, CY, 0.01);
    }

    SUITE("map/north-up-and-rotate");
    {
        float x, y;
        /* Due north, north-up -> straight up. */
        map_project(0, 0, 0.001, 0, 0.0, 1000.0, CX, CY, R, &x, &y);
        CHECK_NEAR(x, CX, 0.5);
        CHECK(y < CY);
        /* Due east, heading-up 90 -> rotated to the top. */
        map_project(0, 0, 0, 0.001, 90.0, 1000.0, CX, CY, R, &x, &y);
        CHECK_NEAR(x, CX, 0.5);
        CHECK(y < CY);
        /* Due east, north-up, beyond range -> NOT clamped (x past the rim). */
        map_project(0, 0, 0, 0.02, 0.0, 100.0, CX, CY, R, &x, &y);
        CHECK(x - CX > R);     /* unclamped: lands outside the circle */
    }

    /* ---- segment / circle clip ----------------------------------------- */
    SUITE("map/clip");
    {
        float ax, ay, bx, by;
        const float cx = 50, cy = 50, r = 40;

        /* Fully inside: returned unchanged. */
        CHECK(map_clip_segment_px(40, 50, 60, 50, cx, cy, r, &ax, &ay, &bx, &by));
        CHECK_NEAR(ax, 40, 0.01); CHECK_NEAR(bx, 60, 0.01);

        /* Fully outside. */
        CHECK(!map_clip_segment_px(100, 100, 120, 120, cx, cy, r, &ax, &ay, &bx, &by));

        /* One end at the centre, the other far outside -> exits at x = 90. */
        CHECK(map_clip_segment_px(50, 50, 150, 50, cx, cy, r, &ax, &ay, &bx, &by));
        CHECK_NEAR(ax, 50, 0.01);
        CHECK_NEAR(bx, 90, 0.01);

        /* Chord: both ends outside, crosses the circle -> enters 10, exits 90. */
        CHECK(map_clip_segment_px(0, 50, 100, 50, cx, cy, r, &ax, &ay, &bx, &by));
        CHECK_NEAR(ax, 10, 0.01);
        CHECK_NEAR(bx, 90, 0.01);

        /* Degenerate point inside / outside. */
        CHECK(map_clip_segment_px(50, 50, 50, 50, cx, cy, r, &ax, &ay, &bx, &by));
        CHECK(!map_clip_segment_px(200, 200, 200, 200, cx, cy, r, &ax, &ay, &bx, &by));

        /* Tangent at the top (y = 90 touches r=40 circle) -> a single point. */
        CHECK(map_clip_segment_px(0, 90, 100, 90, cx, cy, r, &ax, &ay, &bx, &by));
        CHECK_NEAR(ax, 50, 0.01);
        CHECK_NEAR(ay, 90, 0.01);
    }

    /* ---- view bbox + overlap ------------------------------------------- */
    SUITE("map/bbox");
    {
        map_bbox_e7_t v;
        map_view_bbox(47.0, 8.0, 1000.0, 1.0, &v);
        /* 1 km ~ 0.008993 deg lat; lon wider by 1/cos(47). */
        CHECK_NEAR((v.max_lat_e7 - v.min_lat_e7) / 1e7, 2 * 0.008993, 0.0005);
        CHECK_NEAR((v.max_lon_e7 - v.min_lon_e7) / 1e7, 2 * 0.013186, 0.0005);
        CHECK(v.min_lat_e7 < 470000000 && v.max_lat_e7 > 470000000);

        /* Margin enlarges the box. */
        map_bbox_e7_t vw;
        map_view_bbox(47.0, 8.0, 1000.0, 1.5, &vw);
        CHECK(vw.max_lat_e7 > v.max_lat_e7);
        CHECK(vw.min_lat_e7 < v.min_lat_e7);

        map_bbox_e7_t a = {100, 100, 200, 200};
        map_bbox_e7_t b = {150, 150, 300, 300};   /* overlaps a */
        map_bbox_e7_t c = {300, 300, 400, 400};   /* disjoint from a */
        map_bbox_e7_t e = {200, 200, 250, 250};   /* edge-touches a */
        CHECK(map_bbox_overlap(&a, &b));
        CHECK(!map_bbox_overlap(&a, &c));
        CHECK(map_bbox_overlap(&a, &e));           /* inclusive edges */

        /* Margin rescue: a feature just outside the unmargined view still
         * overlaps the margined view box. */
        map_bbox_e7_t feat = {v.max_lat_e7 + 1000, 80000000,
                              v.max_lat_e7 + 2000, 80000001};
        CHECK(!map_bbox_overlap(&v, &feat));
        CHECK(map_bbox_overlap(&vw, &feat));

        /* Extreme near-pole view: the e7 box must saturate, not overflow/collapse. */
        map_bbox_e7_t pole;
        map_view_bbox(89.999, 179.99, 5000.0, 1.15, &pole);
        CHECK(pole.min_lat_e7 <= pole.max_lat_e7);
        CHECK(pole.min_lon_e7 <= pole.max_lon_e7);
        CHECK_EQI(pole.max_lon_e7, 1800000000);
        CHECK_EQI(pole.min_lon_e7, -1800000000);
    }

    /* ---- format: pure decoders + file round-trip ----------------------- */
    SUITE("map/format-parse");
    {
        uint8_t buf[256];
        int o = 0;
        o = put_u32(buf, o, VMAP_MAGIC);
        buf[o++] = VMAP_VERSION;
        buf[o++] = 0;                      /* flags */
        o = put_u16(buf, o, 0);            /* reserved */
        o = put_i32(buf, o, 470000000);    /* global bbox */
        o = put_i32(buf, o, 80000000);
        o = put_i32(buf, o, 471000000);
        o = put_i32(buf, o, 81000000);
        o = put_u32(buf, o, 3);            /* feature_count */
        CHECK_EQI(o, VMAP_HEADER_SIZE);

        /* feature 0: ROAD, 3 points */
        buf[o++] = VMAP_CLASS_ROAD; buf[o++] = 0;
        o = put_u16(buf, o, 3);
        o = put_i32(buf, o, 470000000); o = put_i32(buf, o, 80000000);
        o = put_i32(buf, o, 470500000); o = put_i32(buf, o, 80500000);
        o = put_i32(buf, o, 470000000); o = put_i32(buf, o, 80000000);
        o = put_i32(buf, o, 470250000); o = put_i32(buf, o, 80250000);
        o = put_i32(buf, o, 470500000); o = put_i32(buf, o, 80500000);

        /* feature 1: WATER, 2 points */
        buf[o++] = VMAP_CLASS_WATER; buf[o++] = 0;
        o = put_u16(buf, o, 2);
        o = put_i32(buf, o, 470600000); o = put_i32(buf, o, 80600000);
        o = put_i32(buf, o, 470900000); o = put_i32(buf, o, 80900000);
        o = put_i32(buf, o, 470600000); o = put_i32(buf, o, 80600000);
        o = put_i32(buf, o, 470900000); o = put_i32(buf, o, 80900000);

        /* feature 2: unknown class 99, 2 points (must iterate, not reject) */
        buf[o++] = 99; buf[o++] = 0;
        o = put_u16(buf, o, 2);
        o = put_i32(buf, o, 470100000); o = put_i32(buf, o, 80100000);
        o = put_i32(buf, o, 470200000); o = put_i32(buf, o, 80200000);
        o = put_i32(buf, o, 470100000); o = put_i32(buf, o, 80100000);
        o = put_i32(buf, o, 470200000); o = put_i32(buf, o, 80200000);
        int total = o;

        /* Pure header/feature decoders. */
        vmap_header_t h;
        CHECK_EQI(vmap_parse_header(buf, total, &h), VMAP_HEADER_SIZE);
        CHECK_EQI(h.version, VMAP_VERSION);
        CHECK_EQI(h.feature_count, 3);
        CHECK_EQI(h.bbox.min_lat_e7, 470000000);
        CHECK_EQI(h.bbox.max_lon_e7, 81000000);

        vmap_feature_t ft;
        CHECK_EQI(vmap_parse_feature(buf + VMAP_HEADER_SIZE,
                                     total - VMAP_HEADER_SIZE, &ft), VMAP_FEATURE_PREFIX);
        CHECK_EQI(ft.cls, VMAP_CLASS_ROAD);
        CHECK_EQI(ft.point_count, 3);

        /* Bad magic is rejected. */
        uint8_t bad[VMAP_HEADER_SIZE];
        memcpy(bad, buf, VMAP_HEADER_SIZE);
        bad[0] ^= 0xff;
        CHECK(vmap_parse_header(bad, VMAP_HEADER_SIZE, &h) < 0);

        /* Write the buffer to a temp file and stream it back. */
        const char *path = "ht_vmap_tmp.bin";
        FILE *wf = fopen(path, "wb");
        CHECK(wf != NULL);
        if (wf) { fwrite(buf, 1, (size_t)total, wf); fclose(wf); }

        SUITE("map/format-read");
        int32_t pts[2 * VMAP_MAX_POINTS];
        vmap_reader_t r;
        CHECK_EQI(vmap_open(&r, path), 0);
        CHECK_EQI(r.hdr.feature_count, 3);

        vmap_feature_t f0;
        CHECK_EQI(vmap_next(&r, &f0), 1);
        CHECK_EQI(f0.cls, VMAP_CLASS_ROAD);
        CHECK_EQI(f0.point_count, 3);
        CHECK_EQI(vmap_read_points(&r, &f0, pts, (int)(sizeof pts / sizeof pts[0])), 3);
        CHECK_EQI(pts[0], 470000000); CHECK_EQI(pts[1], 80000000);
        CHECK_EQI(pts[4], 470500000); CHECK_EQI(pts[5], 80500000);

        vmap_feature_t f1;
        CHECK_EQI(vmap_next(&r, &f1), 1);
        CHECK_EQI(f1.cls, VMAP_CLASS_WATER);
        CHECK_EQI(f1.point_count, 2);
        CHECK_EQI(vmap_read_points(&r, &f1, pts, (int)(sizeof pts / sizeof pts[0])), 2);
        CHECK_EQI(pts[0], 470600000); CHECK_EQI(pts[3], 80900000);

        vmap_feature_t f2;
        CHECK_EQI(vmap_next(&r, &f2), 1);
        CHECK_EQI(f2.cls, 99);                 /* unknown class still returned */
        CHECK_EQI(vmap_skip_points(&r, &f2), 0);

        vmap_feature_t f3;
        CHECK_EQI(vmap_next(&r, &f3), 0);      /* EOF */
        vmap_close(&r);

        /* skip-then-read: skipping feature 0 must land read on feature 1. */
        SUITE("map/format-skip");
        CHECK_EQI(vmap_open(&r, path), 0);
        vmap_feature_t g0, g1;
        CHECK_EQI(vmap_next(&r, &g0), 1);
        CHECK_EQI(vmap_skip_points(&r, &g0), 0);
        CHECK_EQI(vmap_next(&r, &g1), 1);
        CHECK_EQI(g1.cls, VMAP_CLASS_WATER);
        CHECK_EQI(vmap_read_points(&r, &g1, pts, (int)(sizeof pts / sizeof pts[0])), 2);
        CHECK_EQI(pts[0], 470600000);
        vmap_close(&r);

        /* A file with bad magic fails to open. */
        FILE *bf = fopen("ht_vmap_bad.bin", "wb");
        if (bf) { fwrite(bad, 1, VMAP_HEADER_SIZE, bf); fclose(bf); }
        CHECK(vmap_open(&r, "ht_vmap_bad.bin") < 0);

        /* Truncated points: prefix promises 3 points but only 1 is present.
         * vmap_next still parses the prefix, but read_points must fail safely. */
        SUITE("map/format-truncated");
        uint8_t tb[80];
        int to = 0;
        to = put_u32(tb, to, VMAP_MAGIC);
        tb[to++] = VMAP_VERSION; tb[to++] = 0; to = put_u16(tb, to, 0);
        to = put_i32(tb, to, 0); to = put_i32(tb, to, 0);
        to = put_i32(tb, to, 0); to = put_i32(tb, to, 0);
        to = put_u32(tb, to, 1);                       /* feature_count = 1 */
        tb[to++] = VMAP_CLASS_ROAD; tb[to++] = 0;
        to = put_u16(tb, to, 3);                       /* claims 3 points... */
        to = put_i32(tb, to, 0); to = put_i32(tb, to, 0);
        to = put_i32(tb, to, 0); to = put_i32(tb, to, 0);
        to = put_i32(tb, to, 470000000); to = put_i32(tb, to, 80000000); /* ...only 1 */
        FILE *tf = fopen("ht_vmap_trunc.bin", "wb");
        if (tf) { fwrite(tb, 1, (size_t)to, tf); fclose(tf); }
        CHECK_EQI(vmap_open(&r, "ht_vmap_trunc.bin"), 0);
        vmap_feature_t tft;
        CHECK_EQI(vmap_next(&r, &tft), 1);
        CHECK(vmap_read_points(&r, &tft, pts, (int)(sizeof pts / sizeof pts[0])) < 0);
        vmap_close(&r);

        remove(path);
        remove("ht_vmap_bad.bin");
        remove("ht_vmap_trunc.bin");
    }
}
