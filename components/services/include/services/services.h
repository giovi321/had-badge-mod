/* Long-lived background services. Each _init wires a driver/feature to the
 * event bus, settings, and (for status) the UI sidebar. */
#ifndef SERVICES_SERVICES_H
#define SERVICES_SERVICES_H

#include "core/settings.h"
#include "core/eventbus.h"

void battery_svc_init(void);
void gps_svc_init(settings_t *reg);
void time_svc_init(void);
void mesh_svc_init(settings_t *reg);
void wifi_svc_init(settings_t *reg);     /* WiFi + web UI from settings */
void status_svc_init(settings_t *reg);   /* creates the sidebar lv_timer */

#endif /* SERVICES_SERVICES_H */
