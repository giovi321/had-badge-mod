/* Launcher home: a horizontally scrollable strip of app tiles. */
#ifndef APPS_LAUNCHER_H
#define APPS_LAUNCHER_H

#include "lvgl.h"
#include "apps/app_iface.h"

typedef void (*launcher_select_cb)(int index);

void launcher_build(lv_obj_t **screen, lv_group_t *group,
                    const app_def_t *const *apps, int n, launcher_select_cb cb);

#endif /* APPS_LAUNCHER_H */
