/* App interface. Each app exposes a const app_def_t* via app_xxx(). The app
 * manager builds the app's screen, routes F-keys/Esc, and ticks it. */
#ifndef APPS_APP_IFACE_H
#define APPS_APP_IFACE_H

#include "lvgl.h"
#include "core/eventbus.h"
#include "core/settings.h"

typedef struct {
    const char *name;
    const char *icon;                /* an LVGL symbol */
    /* Build the app screen and add focusable widgets to `group`. Set *screen. */
    void (*build)(lv_obj_t **screen, lv_group_t *group);
    void (*on_fkey)(int n);          /* F1..F5 pressed (optional) */
    void (*tick)(void);              /* ~5 Hz while active (optional) */
    void (*close)(void);             /* free per-instance state (optional) */
    /* Back/Esc pressed: return true if handled internally (e.g. popped a
     * sub-level), false to let the manager return to the launcher. Optional. */
    bool (*on_back)(void);
} app_def_t;

const app_def_t *app_messages(void);
const app_def_t *app_nodes(void);
const app_def_t *app_settings(void);
const app_def_t *app_gps(void);
const app_def_t *app_diag(void);
const app_def_t *app_track(void);
const app_def_t *app_follow(void);

/* Subscribe the messages app to the event bus + load saved history (call once
 * at boot). */
void messages_init(eventbus_t *bus, settings_t *settings);

/* Set the recipient for the next messages the app sends (0xFFFFFFFF = broadcast).
 * Called by the Nodes app to start a direct message. */
void messages_set_target(uint32_t node);

/* Give the settings app access to the registry (call once at boot). */
void settings_app_init(settings_t *reg);

#endif /* APPS_APP_IFACE_H */
