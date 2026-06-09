/* Great-circle distance and bearing checks. */
#include "test_util.h"
#include "util/geo.h"

void run_geo(void)
{
    SUITE("geo/distance");
    /* one degree of latitude is about 111.19 km */
    CHECK_NEAR(geo_distance_m(0, 0, 1, 0), 111194.9, 50.0);
    CHECK_NEAR(geo_distance_m(0, 0, 0, 0), 0.0, 0.001);

    SUITE("geo/bearing");
    CHECK_NEAR(geo_bearing_deg(0, 0, 1, 0), 0.0, 0.01);     /* due north */
    CHECK_NEAR(geo_bearing_deg(0, 0, 0, 1), 90.0, 0.01);    /* due east  */
    CHECK_NEAR(geo_bearing_deg(0, 0, -1, 0), 180.0, 0.01);  /* due south */
    CHECK_NEAR(geo_bearing_deg(0, 0, 0, -1), 270.0, 0.01);  /* due west  */
}
