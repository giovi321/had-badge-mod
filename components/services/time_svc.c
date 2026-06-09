/* Time service: sets the system clock from the GPS fix once a valid time is
 * available (no RTC on the badge). */
#include "services/services.h"
#include "drivers/gps.h"

#include <sys/time.h>
#include <time.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "time";

static void time_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (time(NULL) < 1700000000) {   /* clock not set yet (< 2023) */
            gps_fix_t fix;
            if (gps_get_fix(&fix) && fix.valid && fix.ts > 1700000000) {
                struct timeval tv = { .tv_sec = (time_t)fix.ts, .tv_usec = 0 };
                settimeofday(&tv, NULL);
                ESP_LOGI(TAG, "clock set from GPS: %lu", (unsigned long)fix.ts);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void time_svc_init(void)
{
    xTaskCreatePinnedToCore(time_task, "time", 2560, NULL, 2, NULL, 1);
}
