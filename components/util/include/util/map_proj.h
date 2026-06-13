/* Project geographic map geometry onto the round radar scope, and the helpers
 * needed to draw it: an unclamped point projection (so lines that cross the rim
 * stay straight), a segment-vs-rim clip, and integer lat/lon bounding-box
 * culling. Portable and host-tested (no LVGL/IDF), like util/radar_proj.h.
 *
 * The projection matches radar_project's orientation (0 deg = scope up, east to
 * the right) so map lines and node blips line up; it differs only in using a
 * cheap local equirectangular approximation and never clamping to the rim. */
#ifndef UTIL_MAP_PROJ_H
#define UTIL_MAP_PROJ_H

#include <stdbool.h>
#include <stdint.h>

/* Lat/lon box in fixed-point 1e-7 degrees (int32 = round(deg * 1e7)) -- the
 * same scale as the .vmap format and node_record_t.lat_i/lon_i. */
typedef struct {
    int32_t min_lat_e7, min_lon_e7, max_lat_e7, max_lon_e7;
} map_bbox_e7_t;

/* Project (plat,plon) onto the scope centred on you (clat,clon). up_deg is the
 * scope's up direction in true degrees (0 = north-up). range_m is the distance
 * the rim represents; (cx,cy) the scope centre in pixels and radius_px the rim
 * radius. Writes the pixel position to (*x,*y). Unlike radar_project the result
 * is NOT clamped to the rim -- a point beyond range lands outside the circle so
 * a line through it keeps its true slope; clip with map_clip_segment_px. */
void map_project(double clat, double clon, double plat, double plon,
                 double up_deg, double range_m,
                 float cx, float cy, float radius_px,
                 float *x, float *y);

/* Clip segment (x0,y0)-(x1,y1) to the rim circle (centre cx,cy, radius r).
 * On overlap, writes the visible sub-segment endpoints to (*vx0,*vy0)-(*vx1,*vy1)
 * and returns true. Returns false when the segment lies entirely outside the
 * circle. A segment fully inside is returned unchanged. */
bool map_clip_segment_px(float x0, float y0, float x1, float y1,
                         float cx, float cy, float r,
                         float *vx0, float *vy0, float *vx1, float *vy1);

/* Lat/lon box (e7) covering the view circle of radius range_m around
 * (clat,clon), enlarged by `margin` (e.g. 1.15) so a feature that only clips
 * the rim is not culled. Does not handle the antimeridian (regional maps). */
void map_view_bbox(double clat, double clon, double range_m, double margin,
                   map_bbox_e7_t *out);

/* True if two e7 boxes overlap (edges inclusive). Pure integer compare. */
bool map_bbox_overlap(const map_bbox_e7_t *a, const map_bbox_e7_t *b);

#endif /* UTIL_MAP_PROJ_H */
