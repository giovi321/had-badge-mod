/* Light sleep + wake sources + backlight dim/off policy. */
#ifndef DRIVERS_POWER_MGMT_H
#define DRIVERS_POWER_MGMT_H

/* Configure DFS/light sleep and GPIO wake on keyboard INT + radio DIO1. */
void power_init(void);

/* Start the backlight dimming task (dims/off on inactivity, restores on key). */
void power_start_backlight_policy(int dim_timeout_s, int off_timeout_s,
                                  int duty_bright, int duty_dim);

#endif /* DRIVERS_POWER_MGMT_H */
