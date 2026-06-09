/* See drivers/power_mgmt.h. */
#include "drivers/power_mgmt.h"
#include "drivers/keyboard.h"
#include "drivers/backlight.h"
#include "board_pins.h"

#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "power";

void power_init(void)
{
#if CONFIG_PM_ENABLE
    /* DFS only for now. Light sleep needs level-triggered GPIO wakeup on the
     * keyboard INT / radio DIO1 pins, which changes those pins' interrupt type
     * to level-triggered and collides with their edge ISRs (DIO1 going high
     * storms the GPIO interrupt -> INT_WDT). DFS still scales 80<->240 MHz for
     * a real power win; proper light sleep is a later refinement. */
    esp_pm_config_t pm = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 80,
        .light_sleep_enable = false,
    };
    if (esp_pm_configure(&pm) == ESP_OK)
        ESP_LOGI(TAG, "DFS enabled (80-240 MHz); light sleep off");
#endif
}

static settings_t *s_bl_reg;
static int bl_cfg(const char *key, int def) { return s_bl_reg ? (int)settings_get_int(s_bl_reg, key) : def; }

static const setting_t BL_SCHEMA[] = {
    {.key = "bl_dim_s", .type = SET_INT, .def = "60", .label = "Dim after (s, 0=off)",
     .group = "Power", .minv = 0, .maxv = 3600, .has_min = true, .has_max = true},
    {.key = "bl_off_s", .type = SET_INT, .def = "0", .label = "Off after (s, 0=off)",
     .group = "Power", .minv = 0, .maxv = 3600, .has_min = true, .has_max = true},
    {.key = "bl_bright", .type = SET_INT, .def = "700", .label = "Bright level",
     .group = "Power", .minv = 10, .maxv = 1023, .has_min = true, .has_max = true},
    {.key = "bl_dim", .type = SET_INT, .def = "120", .label = "Dim level",
     .group = "Power", .minv = 0, .maxv = 1023, .has_min = true, .has_max = true},
};

void power_register_settings(settings_t *reg)
{
    s_bl_reg = reg;
    settings_register_many(reg, BL_SCHEMA, (int)(sizeof BL_SCHEMA / sizeof BL_SCHEMA[0]));
}

static void backlight_task(void *arg)
{
    (void)arg;
    uint32_t last = keyboard_activity_count();
    int idle = 0;
    int stage = 0; /* 0 bright, 1 dim, 2 off */
    backlight_set(bl_cfg("bl_bright", 700));
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        int bright = bl_cfg("bl_bright", 700), dim = bl_cfg("bl_dim", 120);
        int dim_to = bl_cfg("bl_dim_s", 60), off_to = bl_cfg("bl_off_s", 0);
        uint32_t now = keyboard_activity_count();
        if (now != last) {
            last = now;
            idle = 0;
            if (stage != 0) { backlight_set(bright); stage = 0; }
            continue;
        }
        idle++;
        if (stage == 0 && dim_to > 0 && idle >= dim_to) { backlight_set(dim); stage = 1; }
        else if (stage == 1 && off_to > 0 && idle >= off_to) { backlight_set(0); stage = 2; }
    }
}

void power_start_backlight_policy(settings_t *reg)
{
    s_bl_reg = reg;
    xTaskCreatePinnedToCore(backlight_task, "bl", 2560, NULL, 2, NULL, 1);
}
