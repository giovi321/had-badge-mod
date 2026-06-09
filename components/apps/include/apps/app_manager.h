/* App manager: owns the LVGL group + keypad indev, the launcher home, screen
 * switching, and F-key/Esc routing. Runs the UI task. */
#ifndef APPS_APP_MANAGER_H
#define APPS_APP_MANAGER_H

#include "lvgl.h"
#include "core/eventbus.h"
#include "apps/app_iface.h"

void app_manager_init(eventbus_t *bus);
void app_manager_start(void);   /* create + run the UI task */

/* Launch app by index, or return to the launcher home (-1). */
void app_manager_launch(int index);
void app_manager_launch_app(const app_def_t *def);   /* by definition pointer */
void app_manager_go_home(void);

#endif /* APPS_APP_MANAGER_H */
