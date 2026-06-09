/* Backlight PWM (LEDC) on the NV3007 panel. Higher duty = brighter. */
#ifndef DRIVERS_BACKLIGHT_H
#define DRIVERS_BACKLIGHT_H

void backlight_init(void);
void backlight_set(int duty);   /* 0..1023 (10-bit) */
int  backlight_get(void);

#endif /* DRIVERS_BACKLIGHT_H */
