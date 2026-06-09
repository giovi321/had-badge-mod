/* See ui/icons.h. */
#include "ui/icons.h"
#include "ui/theme.h"

const char *icon_battery(int pct, bool charging)
{
    if (charging) return LV_SYMBOL_CHARGE;
    if (pct < 0)  return LV_SYMBOL_BATTERY_EMPTY;
    if (pct >= 90) return LV_SYMBOL_BATTERY_FULL;
    if (pct >= 65) return LV_SYMBOL_BATTERY_3;
    if (pct >= 40) return LV_SYMBOL_BATTERY_2;
    if (pct >= 15) return LV_SYMBOL_BATTERY_1;
    return LV_SYMBOL_BATTERY_EMPTY;
}

lv_obj_t *icon_label(lv_obj_t *parent, const char *sym, long color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, sym);
    lv_obj_set_style_text_color(l, theme_hex(color), 0);
    lv_obj_set_style_text_font(l, theme_font_body(), 0);
    return l;
}
