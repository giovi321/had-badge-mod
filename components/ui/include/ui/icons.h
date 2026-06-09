/* Monochrome icons via LVGL's built-in FontAwesome symbol glyphs. */
#ifndef UI_ICONS_H
#define UI_ICONS_H

#include "lvgl.h"

#define ICON_MESSAGES LV_SYMBOL_ENVELOPE
#define ICON_NODES    LV_SYMBOL_LIST
#define ICON_SETTINGS LV_SYMBOL_SETTINGS
#define ICON_GPS      LV_SYMBOL_GPS
#define ICON_HOME     LV_SYMBOL_HOME
#define ICON_MESH     LV_SYMBOL_BARS
#define ICON_WIFI     LV_SYMBOL_WIFI

/* Battery glyph for a percentage (or charge bolt while charging). */
const char *icon_battery(int pct, bool charging);

/* Create an icon label on parent, recolored. Returns the label. */
lv_obj_t *icon_label(lv_obj_t *parent, const char *sym, long color);

#endif /* UI_ICONS_H */
