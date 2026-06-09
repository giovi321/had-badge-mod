/* See drivers/battery.h. ADC oneshot; disabled unless a sense pin is configured.
 * The stock badge has no confirmed battery divider, so this stays off by default
 * until bat_adc_pin / divider are set from the schematic. */
#include "drivers/battery.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "battery";

#define V_EMPTY 3.30f
#define V_FULL  4.20f

static bool s_enabled;
static adc_oneshot_unit_handle_t s_adc;
static adc_channel_t s_chan;
static float s_div_ratio = 2.0f;

esp_err_t battery_init(int adc_pin, int divider_num, int divider_den)
{
    if (adc_pin < 0) { s_enabled = false; ESP_LOGI(TAG, "battery sense disabled"); return ESP_OK; }

    adc_unit_t unit;
    if (adc_oneshot_io_to_channel(adc_pin, &unit, &s_chan) != ESP_OK) {
        ESP_LOGW(TAG, "gpio %d is not an ADC pin", adc_pin);
        return ESP_ERR_NOT_SUPPORTED;
    }
    adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = unit };
    if (adc_oneshot_new_unit(&ucfg, &s_adc) != ESP_OK) return ESP_FAIL;
    adc_oneshot_chan_cfg_t ccfg = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT };
    adc_oneshot_config_channel(s_adc, s_chan, &ccfg);
    if (divider_den > 0) s_div_ratio = (float)divider_num / (float)divider_den;
    s_enabled = true;
    ESP_LOGI(TAG, "battery sense on gpio %d (divider %.2f)", adc_pin, s_div_ratio);
    return ESP_OK;
}

bool battery_read(battery_state_t *out)
{
    out->present = s_enabled;
    out->pct = -1;
    out->volts = 0.0f;
    out->charging = false;
    out->usb = false;
    if (!s_enabled) return false;

    int raw = 0;
    if (adc_oneshot_read(s_adc, s_chan, &raw) != ESP_OK) return false;
    /* ~3.3V full-scale at 12dB atten, 12-bit; scale by the external divider. */
    float v = (raw / 4095.0f) * 3.3f * s_div_ratio;
    out->volts = v;
    int pct = (int)((v - V_EMPTY) / (V_FULL - V_EMPTY) * 100.0f + 0.5f);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    out->pct = pct;
    return true;
}
