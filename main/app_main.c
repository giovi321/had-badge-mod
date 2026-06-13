/* Communicator badge firmware — boot sequence.
 *
 * Bring-up order: NVS + identity -> settings -> display + theme + chrome ->
 * input -> radio/net -> services -> apps -> power. The UI task is started last
 * so all the LVGL objects are built single-threaded before it runs. */
#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"

#include "app_config.h"
#include "board_pins.h"

#include "core/settings.h"
#include "core/settings_nvs.h"
#include "core/eventbus.h"

#include "drivers/display.h"
#include "drivers/backlight.h"
#include "drivers/keyboard.h"
#include "drivers/power_mgmt.h"

#include "ui/theme.h"
#include "ui/sidebar.h"
#include "ui/menubar.h"

#include "net/backend.h"
#include "radio/radio_task.h"

#include "services/services.h"
#include "services/track.h"
#include "services/ota.h"
#include "ble/ble.h"
#include "apps/app_manager.h"
#include "apps/app_iface.h"

static const char *TAG = "main";

static settings_t s_settings;
static eventbus_t s_bus;

static uint32_t derive_node_id(void)
{
    uint8_t mac[6] = {0};
    esp_efuse_mac_get_default(mac);
    return ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16) |
           ((uint32_t)mac[4] << 8) | mac[5];   /* low 32 bits, Meshtastic style */
}

void app_main(void)
{
    /* 1. NVS + identity + settings registry. */
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    uint32_t node = derive_node_id();
    ESP_LOGI(TAG, "%s %s  node !%08lx", FW_NAME, FW_VERSION, (unsigned long)node);

    eventbus_init(&s_bus);
    settings_init(&s_settings, settings_nvs_create("settings"));
    net_register_settings(&s_settings);   /* radio/device schema */
    power_register_settings(&s_settings); /* backlight/power schema */

    /* Reclaim Bluetooth controller RAM before the display allocates its buffers,
     * unless BLE is enabled. Bluetooth is compiled in but off by default. */
    ble_prepare(&s_settings);

    /* 2. Display + theme + persistent chrome (sidebar + full-width bottom bar). */
    if (display_init() != ESP_OK) ESP_LOGE(TAG, "display init failed");
    theme_init();
    backlight_init();
    sidebar_init();
    menubar_init();

    /* 3. Input. */
    keyboard_init();

    /* 4. Net config + radio. */
    net_init(&s_settings, &s_bus, node);
    if (radio_task_start() != ESP_OK) ESP_LOGW(TAG, "radio unavailable");

    /* 5. Services (register their own settings, start tasks/timers). */
    battery_svc_init();
    gps_svc_init(&s_settings);
    time_svc_init();
    mesh_svc_init(&s_settings);
    track_svc_init();
    wifi_svc_init(&s_settings);
    {
        char sn[16];
        settings_get_str(&s_settings, "short_name", sn, sizeof sn);
        ble_init(&s_settings, sn);
    }

    /* 6. Apps + UI. */
    messages_init(&s_bus, &s_settings);
    radar_init(&s_settings);
    settings_app_init(&s_settings);
    app_manager_init(&s_bus);
    status_svc_init(&s_settings);

    /* 7. Power policy (DFS + backlight dim/off, configurable in Settings). */
    power_start_backlight_policy(&s_settings);
    power_init();

    /* 8. Start the UI task (drives LVGL from here on). */
    app_manager_start();

    /* Reaching here means the image booted cleanly: confirm it so the bootloader
     * keeps it (a no-op unless we just OTA'd and are pending verification). */
    ota_mark_valid();
    ESP_LOGI(TAG, "boot complete");
}
