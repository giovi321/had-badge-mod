/* See ui/theme_map.h and ui/layout.h. Pure logic, no LVGL. */
#include "ui/theme_map.h"
#include "ui/layout.h"
#include "ui/colors.h"
#include <string.h>
#include <math.h>

void ui_fkey_cell(int i, int *x, int *w)
{
    int base = SCREEN_W / FKEY_COUNT;       /* 85 */
    int rem  = SCREEN_W % FKEY_COUNT;        /* 3  -> first 3 cells get +1 */
    int off = 0;
    for (int k = 0; k < i; k++) off += base + (k < rem ? 1 : 0);
    if (x) *x = off;
    if (w) *w = base + (i < rem ? 1 : 0);
}

batt_state_t theme_battery_state(int pct, bool charging, bool usb, bool present)
{
    if (!present) return BATT_NONE;
    if (charging) return BATT_CHARGING;
    if (usb && pct >= 0 && pct >= 99) return BATT_USB_FULL;
    if (pct < 0) return BATT_NONE;
    if (pct < 15) return BATT_CRIT;
    if (pct < 40) return BATT_LOW;
    return BATT_OK;
}

long theme_battery_fill_color(int pct, bool charging, bool present)
{
    if (!present) return C_NONE;
    if (charging) return C_CHARGE;
    if (pct < 0) return C_NONE;
    if (pct < 15) return C_CRIT;
    if (pct < 40) return C_WARN;
    return C_OK;
}

int theme_battery_fill_units(int pct, int inner_max)
{
    if (pct < 0) return 0;
    int u = (int)lround((double)inner_max * pct / 100.0);
    if (u < 1) u = 1;
    if (u > inner_max) u = inner_max;
    return u;
}

int theme_wifi_level(const char *state, int rssi, bool rssi_valid)
{
    if (state == NULL || strcmp(state, "off") == 0 || strcmp(state, "disabled") == 0)
        return 0;
    if (strcmp(state, "scan") == 0) return 1;
    if (strcmp(state, "ap") == 0) return 3;
    if (!rssi_valid) return 1;
    if (rssi >= -60) return 3;
    if (rssi >= -75) return 2;
    return 1;
}

long theme_wifi_color(const char *state)
{
    if (state == NULL) return C_IDLE;
    if (strcmp(state, "ap") == 0) return C_ACCENT;
    if (strcmp(state, "conn") == 0 || strcmp(state, "sta") == 0) return C_OK;
    if (strcmp(state, "scan") == 0) return C_TEXT_DIM;
    return C_IDLE;
}

int theme_mesh_level(bool backend_up, int peers)
{
    if (!backend_up) return 0;
    if (peers <= 0) return 1;
    int l = 1 + peers;
    return l > 3 ? 3 : l;
}

gps_state_t theme_gps_state(bool enabled, bool fix, int sats)
{
    if (!enabled) return GPS_OFF;
    if (!fix) return GPS_SEARCH;
    return sats >= 4 ? GPS_FIX3D : GPS_FIX2D;
}
