/* Pure state -> visual mappers (LVGL-free, host-tested). Ported from the mapper
 * helpers in firmware/badge/ui/theme.py. A pct of -1 means "unknown" (the
 * Python None). Colors are returned as RGB hex, or C_NONE (-1) for "no color". */
#ifndef UI_THEME_MAP_H
#define UI_THEME_MAP_H

#include <stdbool.h>

typedef enum {
    BATT_NONE, BATT_CHARGING, BATT_USB_FULL, BATT_CRIT, BATT_LOW, BATT_OK
} batt_state_t;

typedef enum { GPS_OFF, GPS_SEARCH, GPS_FIX2D, GPS_FIX3D } gps_state_t;

batt_state_t theme_battery_state(int pct, bool charging, bool usb, bool present);
long theme_battery_fill_color(int pct, bool charging, bool present);
int  theme_battery_fill_units(int pct, int inner_max);

/* wifi state strings: "off"/"disabled"/"scan"/"ap"/"conn"/"sta". rssi_valid
 * false means the RSSI is unknown. Returns 0..3 bars. */
int  theme_wifi_level(const char *state, int rssi, bool rssi_valid);
long theme_wifi_color(const char *state);

int  theme_mesh_level(bool backend_up, int peers);   /* 0..3 dots */

gps_state_t theme_gps_state(bool enabled, bool fix, int sats);

#endif /* UI_THEME_MAP_H */
