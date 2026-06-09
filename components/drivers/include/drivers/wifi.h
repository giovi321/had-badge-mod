/* WiFi station / access point control. Off by default. */
#ifndef DRIVERS_WIFI_H
#define DRIVERS_WIFI_H

#include <stdbool.h>
#include "esp_err.h"

void wifi_init(void);

/* Apply a mode: "off", "sta" (join sta_ssid/sta_pass), or "ap" (host
 * ap_ssid/ap_pass, open if the password is empty). Safe to call repeatedly. */
esp_err_t wifi_apply(const char *mode, const char *sta_ssid, const char *sta_pass,
                     const char *ap_ssid, const char *ap_pass);

/* Sidebar state: "off"/"sta"/"conn"/"ap" into buf. rssi is set and rssi_valid
 * true only when a station link is up. */
void wifi_get_state(char *buf, int cap, int *rssi, bool *rssi_valid);

bool wifi_link_up(void);          /* STA got IP, or AP started */
void wifi_ip_str(char *buf, int cap);

#endif /* DRIVERS_WIFI_H */
