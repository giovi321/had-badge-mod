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
    esp_pm_config_t pm = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 80,
        .light_sleep_enable = true,
    };
    if (esp_pm_configure(&pm) == ESP_OK)
        ESP_LOGI(TAG, "DFS + light sleep enabled (80-240 MHz)");
#endif
    /* Wake from light sleep on keyboard INT (active low) and radio DIO1 (rising). */
    gpio_wakeup_enable(PIN_KBD_INT, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(PIN_RF_DIO1, GPIO_INTR_HIGH_LEVEL);
    esp_sleep_enable_gpio_wakeup();
}

typedef struct { int dim_to, off_to, bright, dim; } bl_cfg_t;
static bl_cfg_t s_cfg;

static void backlight_task(void *arg)
{
    (void)arg;
    uint32_t last = keyboard_activity_count();
    int idle = 0;
    int stage = 0; /* 0 bright, 1 dim, 2 off */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint32_t now = keyboard_activity_count();
        if (now != last) {
            last = now;
            idle = 0;
            if (stage != 0) { backlight_set(s_cfg.bright); stage = 0; }
            continue;
        }
        idle++;
        if (stage == 0 && s_cfg.dim_to > 0 && idle >= s_cfg.dim_to) {
            backlight_set(s_cfg.dim); stage = 1;
        } else if (stage == 1 && s_cfg.off_to > 0 && idle >= s_cfg.off_to) {
            backlight_set(0); stage = 2;
        }
    }
}

void power_start_backlight_policy(int dim_timeout_s, int off_timeout_s, int duty_bright, int duty_dim)
{
    s_cfg.dim_to = dim_timeout_s;
    s_cfg.off_to = off_timeout_s;
    s_cfg.bright = duty_bright;
    s_cfg.dim = duty_dim;
    backlight_set(duty_bright);
    xTaskCreatePinnedToCore(backlight_task, "bl", 2560, NULL, 2, NULL, 1);
}
