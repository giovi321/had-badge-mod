/* Light sleep + wake sources + backlight dim/off policy. */
#ifndef DRIVERS_POWER_MGMT_H
#define DRIVERS_POWER_MGMT_H

#include "core/settings.h"

/* Configure DFS (light sleep deferred). */
void power_init(void);

/* Register the Power settings group (dim/off timeouts, bright/dim levels). */
void power_register_settings(settings_t *reg);

/* Start the backlight dimming task. Reads its thresholds live from settings, so
 * changes apply without a reboot. */
void power_start_backlight_policy(settings_t *reg);

#endif /* DRIVERS_POWER_MGMT_H */
