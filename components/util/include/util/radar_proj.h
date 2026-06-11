/* Project a node's position onto a round PPI radar scope. Portable and
 * host-tested (no LVGL/IDF): the app supplies the scope geometry and the
 * current "up" direction, this returns where the blip lands. */
#ifndef UTIL_RADAR_PROJ_H
#define UTIL_RADAR_PROJ_H

#include <stdbool.h>

typedef struct {
    float x, y;          /* blip centre, in scope pixels */
    double dist_m;       /* great-circle distance to the node */
    double bearing_deg;  /* true bearing to the node, 0..360 */
    bool on_scope;       /* false when dist_m > range_m (x,y clamped to the rim) */
} radar_blip_t;

/* Centre (mlat,mlon) is you. up_deg is the scope's up direction in true degrees
 * (0 = north-up; pass your heading for heading-up). range_m is the distance the
 * rim represents. (cx,cy) is the scope centre in pixels and radius_px the rim
 * radius. A node beyond range_m is clamped to the rim with on_scope = false. */
radar_blip_t radar_project(double mlat, double mlon, double nlat, double nlon,
                           double up_deg, double range_m,
                           float cx, float cy, float radius_px);

#endif /* UTIL_RADAR_PROJ_H */
