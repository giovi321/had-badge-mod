/* Status service: an lv_timer (runs in the UI task) that gathers battery / mesh
 * / gps / wifi state into a snapshot and refreshes the sidebar. */
#include "services/services.h"
#include "services/track.h"
#include "lvgl.h"
#include "ui/sidebar.h"
#include "ui/status.h"
#include "drivers/battery.h"
#include "drivers/gps.h"
#include "drivers/wifi.h"
#include "net/backend.h"

static settings_t *s_reg;

static void status_tick(lv_timer_t *t)
{
    (void)t;
    status_snapshot_t s = {0};

    battery_state_t b;
    battery_read(&b);
    s.batt_present = b.present;
    s.batt_pct = b.present ? b.pct : -1;
    s.charging = b.charging;
    s.usb = b.usb;

    static char ws[8];
    int rssi = 0;
    bool rv = false;
    wifi_get_state(ws, sizeof ws, &rssi, &rv);
    s.wifi_state = ws;
    s.wifi_rssi = rssi;
    s.wifi_rssi_valid = rv;

    s.mesh_up = true;
    s.mesh_peers = net_peer_count();

    s.gps_enabled = settings_get_bool(s_reg, "gps_enabled");
    gps_fix_t fix;
    s.gps_fix = gps_get_fix(&fix) && fix.valid;
    s.gps_sats = fix.sats;

    s.tracking = track_is_active();

    sidebar_update(&s);
}

void status_svc_init(settings_t *reg)
{
    s_reg = reg;
    lv_timer_create(status_tick, 1000, NULL);
}
