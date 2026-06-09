/* UI geometry + theme mapper KATs (mirror of firmware/tests/test_theme.py plus
 * new layout invariants for the full-width bottom bar / sidebar / F-keys). */
#include "test_util.h"
#include "ui/layout.h"
#include "ui/theme_map.h"
#include "ui/colors.h"

void run_theme_layout(void)
{
    SUITE("layout/fkey-cells");
    int total = 0, prev_end = 0;
    for (int i = 0; i < FKEY_COUNT; i++) {
        int x, w;
        ui_fkey_cell(i, &x, &w);
        CHECK_EQI(x, prev_end);          /* cells are contiguous */
        CHECK(w == 85 || w == 86);       /* even within 1px */
        total += w;
        prev_end = x + w;
    }
    CHECK_EQI(total, SCREEN_W);          /* full width, no clipping */
    CHECK_EQI(prev_end, SCREEN_W);

    SUITE("layout/chrome");
    CHECK_EQI(SIDEBAR_H, SCREEN_H - BOTTOMBAR_H);  /* sidebar stops at the bar */
    CHECK_EQI(SIDEBAR_H, 124);
    CHECK_EQI(CONTENT_X, 28);
    CHECK_EQI(CONTENT_W, 400);
    CHECK_EQI(BODY_H, 104);

    SUITE("theme/battery-state");
    CHECK_EQI(theme_battery_state(-1, false, false, false), BATT_NONE);
    CHECK_EQI(theme_battery_state(-1, false, true, false), BATT_NONE);
    CHECK_EQI(theme_battery_state(50, false, true, true), BATT_OK);
    CHECK_EQI(theme_battery_state(100, false, true, true), BATT_USB_FULL);
    CHECK_EQI(theme_battery_state(10, false, false, true), BATT_CRIT);
    CHECK_EQI(theme_battery_state(30, false, false, true), BATT_LOW);
    CHECK_EQI(theme_battery_state(80, false, false, true), BATT_OK);
    CHECK_EQI(theme_battery_state(50, true, false, true), BATT_CHARGING);

    SUITE("theme/battery-fill");
    CHECK_EQI(theme_battery_fill_color(-1, false, false), C_NONE);
    CHECK_EQI(theme_battery_fill_color(10, false, true), C_CRIT);
    CHECK_EQI(theme_battery_fill_color(30, false, true), C_WARN);
    CHECK_EQI(theme_battery_fill_color(80, false, true), C_OK);
    CHECK_EQI(theme_battery_fill_color(50, true, true), C_CHARGE);
    CHECK_EQI(theme_battery_fill_units(-1, 12), 0);
    CHECK(theme_battery_fill_units(0, 12) >= 1);
    CHECK_EQI(theme_battery_fill_units(100, 14), 14);
    int u = theme_battery_fill_units(50, 14);
    CHECK(u >= 1 && u <= 14);

    SUITE("theme/wifi");
    CHECK_EQI(theme_wifi_level("off", 0, false), 0);
    CHECK_EQI(theme_wifi_level("ap", 0, false), 3);
    CHECK_EQI(theme_wifi_level("scan", 0, false), 1);
    CHECK_EQI(theme_wifi_level("conn", -55, true), 3);
    CHECK_EQI(theme_wifi_level("conn", -70, true), 2);
    CHECK_EQI(theme_wifi_level("conn", -90, true), 1);
    CHECK_EQI(theme_wifi_color("ap"), C_ACCENT);

    SUITE("theme/mesh");
    CHECK_EQI(theme_mesh_level(false, 5), 0);
    CHECK_EQI(theme_mesh_level(true, 0), 1);
    CHECK_EQI(theme_mesh_level(true, 2), 3);
    CHECK_EQI(theme_mesh_level(true, 10), 3);

    SUITE("theme/gps");
    CHECK_EQI(theme_gps_state(false, true, 9), GPS_OFF);
    CHECK_EQI(theme_gps_state(true, false, 0), GPS_SEARCH);
    CHECK_EQI(theme_gps_state(true, true, 3), GPS_FIX2D);
    CHECK_EQI(theme_gps_state(true, true, 4), GPS_FIX3D);
}
