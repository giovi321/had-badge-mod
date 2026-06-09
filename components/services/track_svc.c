/* See services/track.h. */
#include "services/track.h"
#include "drivers/gps.h"
#include "util/geo.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "track";

#define MIN_MOVE_M   8.0    /* record a point after moving this far ... */
#define MAX_GAP_S    30     /* ... or at least this often while stopped */

static FILE *s_file;
static volatile bool s_active;
static volatile int s_count;
static char s_name[40];
static double s_last_lat, s_last_lon;
static uint32_t s_last_ts;
static SemaphoreHandle_t s_mtx;

bool track_is_active(void) { return s_active; }
int track_point_count(void) { return s_count; }
const char *track_filename(void) { return s_name; }

bool track_start(void)
{
    if (time(NULL) < 1700000000) return false;   /* clock not set */
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    if (s_file) { fclose(s_file); s_file = NULL; }

    time_t now = time(NULL);
    struct tm tmv;
    gmtime_r(&now, &tmv);
    char ts[24];
    strftime(ts, sizeof ts, "%Y%m%d-%H%M%S", &tmv);
    snprintf(s_name, sizeof s_name, "trk_%s.csv", ts);

    char path[64];
    snprintf(path, sizeof path, "%s/%s", TRACK_DIR, s_name);
    s_file = fopen(path, "w");
    if (s_file) {
        fprintf(s_file, "# track %s\n# ts,lat,lon,alt,sats\n", ts);
        fflush(s_file);
    }
    s_count = 0;
    s_last_ts = 0;
    s_active = (s_file != NULL);
    bool ok = s_active;
    xSemaphoreGive(s_mtx);
    ESP_LOGI(TAG, "start %s -> %s", s_name, ok ? "ok" : "failed");
    return ok;
}

void track_stop(void)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_active = false;
    if (s_file) { fclose(s_file); s_file = NULL; }
    xSemaphoreGive(s_mtx);
    ESP_LOGI(TAG, "stop (%d points)", s_count);
}

static void track_task(void *arg)
{
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (!s_active) continue;
        gps_fix_t fix;
        if (!gps_get_fix(&fix) || !fix.valid) continue;

        uint32_t now = fix.ts ? fix.ts : (uint32_t)time(NULL);
        bool take = (s_last_ts == 0);
        if (!take) {
            double d = geo_distance_m(s_last_lat, s_last_lon, fix.lat, fix.lon);
            take = (d >= MIN_MOVE_M) || (now - s_last_ts >= MAX_GAP_S);
        }
        if (!take) continue;

        xSemaphoreTake(s_mtx, portMAX_DELAY);
        if (s_active && s_file) {
            fprintf(s_file, "%lu,%.7f,%.7f,%ld,%d\n", (unsigned long)now,
                    fix.lat, fix.lon, (long)fix.alt, fix.sats);
            fflush(s_file);
            s_count++;
            s_last_lat = fix.lat;
            s_last_lon = fix.lon;
            s_last_ts = now;
        }
        xSemaphoreGive(s_mtx);
    }
}

esp_err_t track_svc_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = TRACK_DIR,
        .partition_label = "storage",
        .max_files = 4,
        .format_if_mount_failed = true,
    };
    esp_err_t e = esp_vfs_spiffs_register(&conf);
    if (e != ESP_OK) ESP_LOGE(TAG, "spiffs mount: %s", esp_err_to_name(e));
    else {
        size_t total = 0, used = 0;
        esp_spiffs_info("storage", &total, &used);
        ESP_LOGI(TAG, "storage mounted: %u KB used of %u KB", (unsigned)(used / 1024), (unsigned)(total / 1024));
    }
    s_mtx = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(track_task, "track", 4096, NULL, 3, NULL, 1);
    return e;
}
