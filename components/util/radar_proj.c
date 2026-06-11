/* See util/radar_proj.h. Distance + bearing come from util/geo; the only extra
 * work is rotating by the scope's up direction and scaling range to pixels. */
#include "util/radar_proj.h"
#include "util/geo.h"

#include <math.h>

#define RP_PI 3.14159265358979323846

radar_blip_t radar_project(double mlat, double mlon, double nlat, double nlon,
                           double up_deg, double range_m,
                           float cx, float cy, float radius_px)
{
    radar_blip_t b;
    b.dist_m = geo_distance_m(mlat, mlon, nlat, nlon);
    b.bearing_deg = geo_bearing_deg(mlat, mlon, nlat, nlon);

    /* Bearing relative to the scope's up direction (north-up when up_deg = 0). */
    double rel = fmod(b.bearing_deg - up_deg + 360.0, 360.0);

    double frac = (range_m > 0.0) ? b.dist_m / range_m : 0.0;
    b.on_scope = (frac <= 1.0);
    if (frac > 1.0) frac = 1.0;     /* clamp out-of-range blips to the rim */

    double rr = frac * (double)radius_px;
    double th = rel * RP_PI / 180.0;
    b.x = (float)((double)cx + rr * sin(th));
    b.y = (float)((double)cy - rr * cos(th));
    return b;
}
