/* TCA8418 matrix keyboard. Decodes the badge key matrix (ported from
 * firmware/badge/hardware/keyboard.py), tracks F-key/Esc/modifier edge state,
 * and feeds a queue drained by the LVGL keypad indev. */
#ifndef DRIVERS_KEYBOARD_H
#define DRIVERS_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* Special key codes match LVGL's LV_KEY_* so they feed the keypad indev
 * directly; printable keys are their ASCII value. F-keys/modifiers use
 * app-only sentinels above 0x200 and never enter the text queue. */
enum {
    K_NONE  = 0,
    K_TAB   = 9,    /* LV_KEY_NEXT  */
    K_ENTER = 10,   /* LV_KEY_ENTER */
    K_BS    = 8,    /* LV_KEY_BACKSPACE */
    K_UP    = 17,   /* LV_KEY_UP    */
    K_DOWN  = 18,   /* LV_KEY_DOWN  */
    K_RIGHT = 19,   /* LV_KEY_RIGHT */
    K_LEFT  = 20,   /* LV_KEY_LEFT  */
    K_ESC   = 27,   /* LV_KEY_ESC   */
    K_DEL   = 127,  /* LV_KEY_DEL   */
    K_F1 = 0x201, K_F2, K_F3, K_F4, K_F5,
    K_SFT = 0x210, K_CTL, K_ALT, K_JW
};

esp_err_t keyboard_init(void);

/* Pop one key code for the LVGL indev (0 timeout). Returns false if empty. */
bool keyboard_pop(uint32_t *out);

/* Edge-triggered accelerators (true once per press). n is 1..5 for F1..F5. */
bool keyboard_f_pressed(int n);
bool keyboard_esc_pressed(void);

/* Live modifier state (loose, for apps that care). */
bool keyboard_shift(void);
bool keyboard_ctrl(void);
bool keyboard_alt(void);
bool keyboard_meta(void);

/* Monotonic counter bumped on any key activity (drives backlight dimming). */
uint32_t keyboard_activity_count(void);

#endif /* DRIVERS_KEYBOARD_H */
