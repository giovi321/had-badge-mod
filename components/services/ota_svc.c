/* See services/ota.h. A thin wrapper over esp_ota_ops: open the inactive slot,
 * stream the image in, validate, and set it as the next boot partition. */
#include "services/ota.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "esp_system.h"

static const char *TAG = "ota";

static esp_ota_handle_t s_handle;
static const esp_partition_t *s_part;
static size_t s_written;

void ota_mark_valid(void)
{
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (run && esp_ota_get_state_partition(run, &st) == ESP_OK &&
        st == ESP_OTA_IMG_PENDING_VERIFY) {
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK)
            ESP_LOGI(TAG, "running image confirmed valid");
    }
}

esp_err_t ota_begin(size_t total)
{
    s_part = esp_ota_get_next_update_partition(NULL);
    if (!s_part) { ESP_LOGE(TAG, "no OTA partition"); return ESP_FAIL; }
    s_written = 0;
    esp_err_t e = esp_ota_begin(s_part, total ? total : OTA_SIZE_UNKNOWN, &s_handle);
    if (e != ESP_OK) { s_handle = 0; ESP_LOGE(TAG, "begin: %s", esp_err_to_name(e)); }
    else ESP_LOGI(TAG, "writing to %s (%u bytes)", s_part->label, (unsigned)total);
    return e;
}

esp_err_t ota_write(const void *data, size_t len)
{
    if (!s_handle) return ESP_ERR_INVALID_STATE;
    esp_err_t e = esp_ota_write(s_handle, data, len);
    if (e == ESP_OK) s_written += len;
    return e;
}

esp_err_t ota_end(void)
{
    if (!s_handle) return ESP_ERR_INVALID_STATE;
    esp_err_t e = esp_ota_end(s_handle);
    s_handle = 0;
    if (e != ESP_OK) { ESP_LOGE(TAG, "end: %s", esp_err_to_name(e)); return e; }
    e = esp_ota_set_boot_partition(s_part);
    if (e != ESP_OK) ESP_LOGE(TAG, "set boot: %s", esp_err_to_name(e));
    else ESP_LOGI(TAG, "boot set to %s, %u bytes", s_part->label, (unsigned)s_written);
    return e;
}

void ota_abort(void)
{
    if (s_handle) { esp_ota_abort(s_handle); s_handle = 0; }
}

static void reboot_cb(void *arg) { (void)arg; esp_restart(); }

void ota_reboot_soon(void)
{
    const esp_timer_create_args_t a = { .callback = reboot_cb, .name = "ota_reboot" };
    esp_timer_handle_t t;
    if (esp_timer_create(&a, &t) == ESP_OK) esp_timer_start_once(t, 1500 * 1000);
}
