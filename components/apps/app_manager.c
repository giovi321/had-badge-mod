/* See apps/app_manager.h. */
#include "apps/app_manager.h"
#include "apps/app_iface.h"
#include "apps/launcher.h"
#include "ui/menubar.h"
#include "drivers/keyboard.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "apps";

static const app_def_t *s_apps[6];
static int s_napps;
static lv_group_t *s_group;
static lv_indev_t *s_indev;
static lv_obj_t *s_screen;
static int s_current = -1;   /* -1 = launcher home */
static int s_home_focus = 0; /* remember which launcher tile was selected */

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
    lv_obj_t *old = s_screen;
    s_screen = scr;
    lv_screen_load(scr);
    if (old && old != scr) lv_obj_delete(old);
}

static void on_launch(int index) { app_manager_launch(index); }

void app_manager_go_home(void)
{
    ESP_LOGI(TAG, "-> launcher (focus %d)", s_home_focus);
    if (s_current >= 0 && s_apps[s_current]->close) s_apps[s_current]->close();
    s_current = -1;
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
    s_home_focus = index;          /* land back here when we return home */
    lv_group_remove_all_objs(s_group);
    lv_obj_t *scr = NULL;
    s_apps[index]->build(&scr, s_group);
    if (scr) load_screen(scr);
    menubar_set_cell(4, "Back");   /* F5 = Back is always present in an app */
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
    if (keyboard_esc_pressed() && s_current >= 0) { go_back(); return; }
    for (int n = 1; n <= 5; n++) {
        if (!keyboard_f_pressed(n)) continue;
        if (s_current >= 0) {
            if (n == 5) { go_back(); return; }               /* F5 = Back, always */
            if (s_apps[s_current]->on_fkey) s_apps[s_current]->on_fkey(n);
        } else if (n == 3) {  /* home: F3 = Open focused tile */
            lv_obj_t *f = lv_group_get_focused(s_group);
            if (f) app_manager_launch((int)(intptr_t)lv_obj_get_user_data(f));
        }
    }
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
    s_napps = 5;

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
    xTaskCreatePinnedToCore(ui_task, "ui", 8192, NULL, 4, NULL, 1);
}
