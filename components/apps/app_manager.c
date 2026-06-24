/* See apps/app_manager.h. */
#include "apps/app_manager.h"
#include "apps/app_iface.h"
#include "apps/launcher.h"
#include "ui/menubar.h"
#include "drivers/keyboard.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "apps";

static const app_def_t *s_apps[12];
static int s_napps;
static lv_group_t *s_group;
static lv_indev_t *s_indev;
static lv_obj_t *s_screen;
static int s_current = -1;   /* -1 = launcher home */
static int s_home_focus = 0; /* remember which launcher tile was selected */

/* Auto-hide bottom bar (apps that set autohide_bar, e.g. Messages). */
#define BAR_HIDE_INIT_MS  2000   /* hide this long after entering the app */
#define BAR_HIDE_AFTER_MS 3000   /* stay this long after a reveal/interaction */
static bool s_bar_auto;
static bool s_bar_hidden;
static uint32_t s_bar_since;
static uint32_t s_bar_delay;

static void bar_hide(void)
{
    if (!s_bar_auto || s_bar_hidden || s_current < 0) return;
    s_bar_hidden = true;
    menubar_set_visible(false, true);
    if (s_apps[s_current]->on_bar) s_apps[s_current]->on_bar(false);
}

static void bar_reveal(void)
{
    s_bar_hidden = false;
    s_bar_since = lv_tick_get();
    s_bar_delay = BAR_HIDE_AFTER_MS;
    menubar_set_visible(true, true);
    if (s_current >= 0 && s_apps[s_current]->on_bar) s_apps[s_current]->on_bar(true);
}

/* LVGL keypad indev: emit each queued key as a press then a release. */
static void kp_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    static uint32_t held;
    static bool pressed;
    if (pressed) {
        data->key = held;
        data->state = LV_INDEV_STATE_RELEASED;
        pressed = false;
        data->continue_reading = true;
        return;
    }
    uint32_t k;
    if (keyboard_pop(&k)) {
        held = k;
        data->key = k;
        data->state = LV_INDEV_STATE_PRESSED;
        pressed = true;
        data->continue_reading = true;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        data->continue_reading = false;
    }
}

static void load_screen(lv_obj_t *scr)
{
    /* If this swap was triggered from inside a key event (ENTER on a list row or
     * tile), swallow the in-flight key release: otherwise LVGL delivers it to the
     * new screen's auto-focused first widget as a CLICK (e.g. opening Nodes
     * "clicked" its first row and jumped straight into Messages). */
    if (lv_indev_get_active_obj()) lv_indev_wait_release(s_indev);

    lv_obj_t *old = s_screen;
    s_screen = scr;
    lv_screen_load(scr);
    if (old && old != scr) lv_obj_delete(old);
}

/* Launcher-initiated: remember the tile so Back lands on it. Programmatic
 * app-to-app jumps (Nodes -> Messages) must NOT move the home focus. */
static void on_launch(int index) { s_home_focus = index; app_manager_launch(index); }

void app_manager_go_home(void)
{
    ESP_LOGI(TAG, "-> launcher (focus %d)", s_home_focus);
    if (s_current >= 0 && s_apps[s_current]->close) s_apps[s_current]->close();
    s_current = -1;
    s_bar_auto = false;
    s_bar_hidden = false;
    menubar_set_visible(true, false);   /* always show the bar on the launcher */
    lv_group_remove_all_objs(s_group);
    lv_obj_t *scr = NULL;
    launcher_build(&scr, s_group, s_apps, s_napps, on_launch, s_home_focus);
    load_screen(scr);
}

void app_manager_launch(int index)
{
    if (index < 0 || index >= s_napps) return;
    ESP_LOGI(TAG, "-> launch %d (%s)", index, s_apps[index]->name);
    if (s_current >= 0 && s_apps[s_current]->close) s_apps[s_current]->close();
    s_current = index;
    lv_group_remove_all_objs(s_group);
    lv_obj_t *scr = NULL;
    s_apps[index]->build(&scr, s_group);
    if (scr) load_screen(scr);
    menubar_set_cell(4, "Back");   /* F5 = Back is always present in an app */

    /* Reset the auto-hide bar for the newly entered app. */
    s_bar_auto = s_apps[index]->autohide_bar;
    s_bar_hidden = false;
    menubar_set_visible(true, false);
    s_bar_since = lv_tick_get();
    s_bar_delay = BAR_HIDE_INIT_MS;
}

void app_manager_launch_app(const app_def_t *def)
{
    for (int i = 0; i < s_napps; i++)
        if (s_apps[i] == def) { app_manager_launch(i); return; }
}

static void go_back(void)
{
    /* Let the app pop an internal level first (Settings sub-menus); otherwise
     * return straight to the launcher. */
    if (s_current >= 0 && s_apps[s_current]->on_back && s_apps[s_current]->on_back()) return;
    app_manager_go_home();
}

static void manager_tick(lv_timer_t *t)
{
    (void)t;
    /* Drain every edge flag exactly once per tick, so a key pressed just before
     * a navigation cannot fire a tick later against the wrong screen. */
    bool esc = keyboard_esc_pressed();
    bool fk[5];
    for (int n = 1; n <= 5; n++) fk[n - 1] = keyboard_f_pressed(n);

    if ((esc || fk[4]) && s_current >= 0) {
        /* Esc and F5/Back are literally the same action: always one press,
         * regardless of the auto-hidden bar (which go_home re-shows anyway). */
        go_back();
    } else if (s_current < 0) {
        if (fk[0]) {  /* home: F1 = Open focused tile */
            lv_obj_t *f = lv_group_get_focused(s_group);
            if (f) on_launch((int)(intptr_t)lv_obj_get_user_data(f));
        }
    } else {
        for (int n = 1; n <= 4; n++) {
            if (!fk[n - 1]) continue;
            /* Bar auto-hidden: the first app-action key only reveals the bar (so
             * the labels can be read), WITHOUT executing. Press again, with the
             * bar visible, to run the function. Back (F5 / Esc) still exits in one
             * press, handled above. */
            if (s_bar_auto && s_bar_hidden) { bar_reveal(); break; }
            if (s_bar_auto) { s_bar_since = lv_tick_get(); s_bar_delay = BAR_HIDE_AFTER_MS; }
            int cur = s_current;
            if (s_apps[s_current]->on_fkey) s_apps[s_current]->on_fkey(n);
            if (s_current != cur) break;   /* app switched: stop dispatching */
        }
    }
    if (s_bar_auto && !s_bar_hidden && s_current >= 0 &&
        (uint32_t)(lv_tick_get() - s_bar_since) >= s_bar_delay)
        bar_hide();
    if (s_current >= 0 && s_apps[s_current]->tick) s_apps[s_current]->tick();
}

void app_manager_init(eventbus_t *bus)
{
    (void)bus;
    s_apps[0] = app_messages();
    s_apps[1] = app_nodes();
    s_apps[2] = app_settings();
    s_apps[3] = app_diag();
    s_apps[4] = app_gps();
    s_apps[5] = app_track();
    s_apps[6] = app_follow();
    s_apps[7] = app_packets();
    s_apps[8] = app_tracker();
    s_apps[9] = app_radar();
    s_napps = 10;

    s_group = lv_group_create();
    lv_group_set_default(s_group);

    s_indev = lv_indev_create();
    lv_indev_set_type(s_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(s_indev, kp_read);
    lv_indev_set_group(s_indev, s_group);

    lv_timer_create(manager_tick, 100, NULL);
    app_manager_go_home();
    ESP_LOGI(TAG, "app manager ready (%d apps)", s_napps);
}

static void ui_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t next = lv_timer_handler();
        if (next == LV_NO_TIMER_READY || next > 50) next = 50;
        if (next < 1) next = 1;
        vTaskDelay(pdMS_TO_TICKS(next));
    }
}

void app_manager_start(void)
{
    /* The UI task stack must be internal RAM (it runs NVS/flash writes, so a PSRAM
     * stack would fault while the cache is disabled). If WiFi has eaten internal
     * RAM and this allocation fails, the UI silently never renders -- so fail loud
     * rather than boot to a blank screen with no clue why. */
    if (xTaskCreatePinnedToCore(ui_task, "ui", 8192, NULL, 4, NULL, 1) != pdPASS)
        ESP_LOGE(TAG, "UI task create failed (internal RAM low: %u free) -- screen will be blank",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
}
