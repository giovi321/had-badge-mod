/* Mesh service: announces this node (NodeInfo) on join and periodically, and
 * shares GPS position when enabled. */
#include "services/services.h"
#include "net/backend.h"
#include "drivers/gps.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "mesh_svc";

#define LOOP_S        5
#define NODEINFO_S    300   /* re-announce every 5 min */

static settings_t *s_reg;

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

        if (since_info >= NODEINFO_S) { net_send_nodeinfo(); since_info = 0; }

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
