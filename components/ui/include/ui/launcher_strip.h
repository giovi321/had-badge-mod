/* Horizontally scrollable icon strip for the app launcher. Each tile shows a
 * monochrome icon and a single-line label (ellipsised, never wrapped). Left/
 * Right move the selection (auto-scrolling into view); Enter activates. */
#ifndef UI_LAUNCHER_STRIP_H
#define UI_LAUNCHER_STRIP_H

#include "lvgl.h"

typedef struct {
    const char *name;
    const char *icon;   /* an LVGL symbol */
} strip_item_t;

typedef void (*strip_select_cb)(int index, void *ctx);

lv_obj_t *launcher_strip_create(lv_obj_t *parent, const strip_item_t *items, int n,
                                strip_select_cb cb, void *ctx, lv_group_t *group,
                                int initial_focus);

#endif /* UI_LAUNCHER_STRIP_H */
