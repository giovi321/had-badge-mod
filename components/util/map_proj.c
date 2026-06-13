/* See util/map_proj.h. The projection is a local equirectangular (tangent
 * plane) mapping: at radar ranges (<= 5 km) it agrees with radar_project's
 * great-circle placement to well under a pixel, while costing a couple of
 * multiplies per point instead of a haversine. */
#include "util/map_proj.h"

#include <math.h>

#define MP_PI      3.14159265358979323846
#define MP_D2R     (MP_PI / 180.0)
#define MP_R_EARTH 6371000.0

void map_project(double clat, double clon, double plat, double plon,
                 double up_deg, double range_m,
                 float cx, float cy, float radius_px,
                 float *x, float *y)
{
    /* Offset from you, in metres on the local tangent plane. */
    double north_m = MP_R_EARTH * (plat - clat) * MP_D2R;
    double east_m  = MP_R_EARTH * cos(clat * MP_D2R) * (plon - clon) * MP_D2R;

    /* Rotate so the scope's up direction points to the top of the screen. */
    double a  = up_deg * MP_D2R;
    double sa = sin(a), ca = cos(a);
    double u =  north_m * ca + east_m * sa;   /* component toward scope top   */
    double v =  east_m  * ca - north_m * sa;  /* component toward scope right */

    double scale = (range_m > 0.0) ? (double)radius_px / range_m : 0.0;
    *x = (float)((double)cx + v * scale);
    *y = (float)((double)cy - u * scale);     /* screen y grows downward */
}

bool map_clip_segment_px(float x0, float y0, float x1, float y1,
                         float cx, float cy, float r,
                         float *vx0, float *vy0, float *vx1, float *vy1)
{
    double dx = (double)x1 - x0, dy = (double)y1 - y0;
    double fx = (double)x0 - cx, fy = (double)y0 - cy;
    double A = dx * dx + dy * dy;
    double B = 2.0 * (fx * dx + fy * dy);
    double C = fx * fx + fy * fy - (double)r * (double)r;

    if (A <= 1e-12) {
        /* Zero-length segment: visible iff the point is within the circle. */
        if (C > 0.0) return false;
        *vx0 = x0; *vy0 = y0; *vx1 = x1; *vy1 = y1;
        return true;
    }

    double disc = B * B - 4.0 * A * C;
    if (disc < 0.0) return false;             /* line misses the circle */

    double sq = sqrt(disc);
    double ta = (-B - sq) / (2.0 * A);        /* enters the circle */
    double tb = (-B + sq) / (2.0 * A);        /* leaves the circle */

    /* Intersect the inside interval [ta,tb] with the segment domain [0,1]. */
    double t0 = ta > 0.0 ? ta : 0.0;
    double t1 = tb < 1.0 ? tb : 1.0;
    if (t1 < t0) return false;                /* nothing of the segment is inside */

    *vx0 = (float)(x0 + t0 * dx);
    *vy0 = (float)(y0 + t0 * dy);
    *vx1 = (float)(x0 + t1 * dx);
    *vy1 = (float)(y0 + t1 * dy);
    return true;
}

void map_view_bbox(double clat, double clon, double range_m, double margin,
                   map_bbox_e7_t *out)
{
    double rad = range_m * margin;
    double dlat = rad / (MP_R_EARTH * MP_D2R);
    double coslat = cos(clat * MP_D2R);
    if (coslat < 1e-6) coslat = 1e-6;         /* guard near the poles */
    double dlon = rad / (MP_R_EARTH * coslat * MP_D2R);

    /* Saturate to the e7 range so an extreme (near-pole) box can't overflow the
     * int32 cast -- which would be undefined behaviour. ±180 deg = ±1.8e9. */
    out->min_lat_e7 = (int32_t)lround(fmax(-1.8e9, fmin(1.8e9, (clat - dlat) * 1e7)));
    out->max_lat_e7 = (int32_t)lround(fmax(-1.8e9, fmin(1.8e9, (clat + dlat) * 1e7)));
    out->min_lon_e7 = (int32_t)lround(fmax(-1.8e9, fmin(1.8e9, (clon - dlon) * 1e7)));
    out->max_lon_e7 = (int32_t)lround(fmax(-1.8e9, fmin(1.8e9, (clon + dlon) * 1e7)));
}

bool map_bbox_overlap(const map_bbox_e7_t *a, const map_bbox_e7_t *b)
{
    if (a->max_lat_e7 < b->min_lat_e7 || a->min_lat_e7 > b->max_lat_e7) return false;
    if (a->max_lon_e7 < b->min_lon_e7 || a->min_lon_e7 > b->max_lon_e7) return false;
    return true;
}
