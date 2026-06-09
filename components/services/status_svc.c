/* Status service: an lv_timer (runs in the UI task) that gathers battery / mesh
 * / gps / wifi state into a snapshot and refreshes the sidebar. */
#include "services/services.h"
#include "ui/sidebar.h"
#include "ui/status.h"
#include "drivers/battery.h"
#include "drivers/gps.h"
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

    s.wifi_state = "off";          /* WiFi not used in v1 */
    s.wifi_rssi_valid = false;

    s.mesh_up = true;
    s.mesh_peers = net_peer_count();

    s.gps_enabled = settings_get_bool(s_reg, "gps_enabled");
    gps_fix_t fix;
    s.gps_fix = gps_get_fix(&fix) && fix.valid;
    s.gps_sats = fix.sats;

    sidebar_update(&s);
}

void status_svc_init(settings_t *reg)
{
    s_reg = reg;
    lv_timer_create(status_tick, 1000, NULL);
}
