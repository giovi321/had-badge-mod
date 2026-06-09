/* NV3007 display bring-up: SPI panel IO + LVGL display (lv_nv3007_create).
 * Owns the SPI bus, draw buffers, LVGL tick, and the flush path. */
#ifndef DRIVERS_DISPLAY_H
#define DRIVERS_DISPLAY_H

#include "esp_err.h"
#include "lvgl.h"

/* Initialise the panel and create the LVGL display (428x142 logical, rot 270).
 * Calls lv_init() if not already initialised. */
esp_err_t display_init(void);

/* The LVGL display handle (NULL before display_init). */
lv_display_t *display_handle(void);

#endif /* DRIVERS_DISPLAY_H */
