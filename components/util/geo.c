/* See util/geo.h. */
#include "util/geo.h"
#include <math.h>

#define GEO_PI 3.14159265358979323846
#define D2R (GEO_PI / 180.0)
#define R_EARTH 6371000.0

double geo_distance_m(double lat1, double lon1, double lat2, double lon2)
{
    double dlat = (lat2 - lat1) * D2R;
    double dlon = (lon2 - lon1) * D2R;
    double a = sin(dlat / 2) * sin(dlat / 2) +
               cos(lat1 * D2R) * cos(lat2 * D2R) * sin(dlon / 2) * sin(dlon / 2);
    return R_EARTH * 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
}

double geo_bearing_deg(double lat1, double lon1, double lat2, double lon2)
{
    double dlon = (lon2 - lon1) * D2R;
    double y = sin(dlon) * cos(lat2 * D2R);
    double x = cos(lat1 * D2R) * sin(lat2 * D2R) -
               sin(lat1 * D2R) * cos(lat2 * D2R) * cos(dlon);
    double b = atan2(y, x) / D2R;
    return fmod(b + 360.0, 360.0);
}
