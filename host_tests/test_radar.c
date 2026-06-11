/* Radar PPI projection checks (util/radar_proj). Scope: centre (50,50), 40 px
 * rim. Small lat/lon offsets near the equator give clean N/E/S/W bearings and
 * ~111 m per 0.001 degree. */
#include "test_util.h"
#include "util/radar_proj.h"

void run_radar(void)
{
    const float CX = 50, CY = 50, R = 40;

    SUITE("radar/north-up");
    /* Due north, well inside range -> straight up: x == CX, y above centre. */
    radar_blip_t north = radar_project(0, 0, 0.001, 0, 0, 1000.0, CX, CY, R);
    CHECK_NEAR(north.bearing_deg, 0.0, 0.1);
    CHECK_NEAR(north.x, CX, 0.5);
    CHECK(north.y < CY);
    CHECK(north.on_scope);

    /* Due east -> to the right: x above centre, y == CY. */
    radar_blip_t east = radar_project(0, 0, 0, 0.001, 0, 1000.0, CX, CY, R);
    CHECK_NEAR(east.bearing_deg, 90.0, 0.1);
    CHECK(east.x > CX);
    CHECK_NEAR(east.y, CY, 0.5);
    CHECK(east.on_scope);

    SUITE("radar/range-scale");
    double d = east.dist_m;     /* ~111 m */
    /* On a scope whose rim == the node's distance, the east node sits on +R. */
    radar_blip_t rim = radar_project(0, 0, 0, 0.001, 0, d, CX, CY, R);
    CHECK_NEAR(rim.x - CX, R, 0.5);
    CHECK(rim.on_scope);
    /* On a 1 km scope it sits proportionally closer in. */
    radar_blip_t in = radar_project(0, 0, 0, 0.001, 0, 1000.0, CX, CY, R);
    CHECK_NEAR(in.x - CX, R * (float)(d / 1000.0), 0.5);

    SUITE("radar/heading-up");
    /* Facing east (up = 90) rotates a due-east node to the top of the scope. */
    radar_blip_t hu = radar_project(0, 0, 0, 0.001, 90.0, 1000.0, CX, CY, R);
    CHECK_NEAR(hu.x, CX, 0.5);
    CHECK(hu.y < CY);

    SUITE("radar/beyond-range");
    /* ~1.1 km east on a 100 m scope: off-scope, clamped to the east rim. */
    radar_blip_t farb = radar_project(0, 0, 0, 0.01, 0, 100.0, CX, CY, R);
    CHECK(!farb.on_scope);
    CHECK_NEAR(farb.x - CX, R, 0.5);
    CHECK_NEAR(farb.y, CY, 0.5);

    SUITE("radar/self");
    /* Your own position lands dead centre. */
    radar_blip_t me = radar_project(1.23, 4.56, 1.23, 4.56, 0, 1000.0, CX, CY, R);
    CHECK_NEAR(me.x, CX, 0.01);
    CHECK_NEAR(me.y, CY, 0.01);
    CHECK(me.on_scope);
}
