/* Mesh service: announces this node (NodeInfo) on join and periodically, and
 * shares GPS position when enabled. */
#include "services/services.h"
#include "net/backend.h"
#include "drivers/gps.h"
#include "drivers/battery.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "mesh_svc";

#define LOOP_S        5

static settings_t *s_reg;

/* NodeInfo + telemetry: tell the mesh this node exists and how it's doing. */
static void send_node_beacon(void)
{
    net_send_nodeinfo();
    battery_state_t b;
    battery_read(&b);
    net_send_telemetry(b.present ? b.pct : -1, b.present ? b.volts : 0.0f,
                       (uint32_t)(esp_timer_get_time() / 1000000));
}

/* Manual "announce now" (from a button); also shares position if enabled. */
void mesh_svc_announce_now(void)
{
    send_node_beacon();
    gps_fix_t fix;
    if (s_reg && settings_get_bool(s_reg, "mesh_share_pos") && gps_get_fix(&fix) && fix.valid)
        net_send_position(fix.lat, fix.lon, fix.alt, fix.ts);
    ESP_LOGI(TAG, "manual announce");
}

static void mesh_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(3000));     /* let the radio settle */
    net_send_nodeinfo();
    ESP_LOGI(TAG, "announced node");

    int since_info = 0, since_pos = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(LOOP_S * 1000));
        since_info += LOOP_S;
        since_pos += LOOP_S;

        int info_int = (int)settings_get_int(s_reg, "mesh_info_int");
        if (info_int < 30) info_int = 30;
        if (since_info >= info_int) {
            send_node_beacon();
            since_info = 0;
        }

        if (settings_get_bool(s_reg, "mesh_share_pos")) {
            int interval = (int)settings_get_int(s_reg, "mesh_pos_int");
            gps_fix_t fix;
            if (since_pos >= interval && gps_get_fix(&fix) && fix.valid) {
                net_send_position(fix.lat, fix.lon, fix.alt, fix.ts);
                since_pos = 0;
            }
        }
    }
}

void mesh_svc_init(settings_t *reg)
{
    s_reg = reg;
    xTaskCreatePinnedToCore(mesh_task, "mesh_svc", 4096, NULL, 3, NULL, 0);
}
