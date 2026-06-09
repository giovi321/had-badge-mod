/* Shared status snapshot updated by services and rendered by the sidebar. */
#ifndef UI_STATUS_H
#define UI_STATUS_H

#include <stdbool.h>

typedef struct {
    bool batt_present;
    int  batt_pct;          /* -1 unknown */
    bool charging;
    bool usb;
    const char *wifi_state;  /* "off"/"scan"/"ap"/"conn"/"sta" */
    int  wifi_rssi;
    bool wifi_rssi_valid;
    bool mesh_up;
    int  mesh_peers;
    bool gps_enabled;
    bool gps_fix;
    int  gps_sats;
    bool tracking;          /* breadcrumb recording active */
} status_snapshot_t;

#endif /* UI_STATUS_H */
