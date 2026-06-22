/* See drivers/gps.h. UART RX -> NMEA parser -> latest fix snapshot. */
#include "drivers/gps.h"
#include "board_pins.h"
#include "util/nmea.h"

#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

static const char *TAG = "gps";
static gps_fix_t s_fix;
static volatile bool s_run;
static nmea_parser_t s_parser;

/* Activity counters for diagnostics. Timestamps are esp_timer micros; 0 = never. */
static uint32_t s_bytes, s_sentences;
static int64_t s_last_data_us, s_last_fix_us;

static void on_sentence(const nmea_result_t *r, void *ctx)
{
    (void)ctx;
    s_sentences++;
    if (r->kind == NMEA_GSV) {
        s_fix.sats_in_view = r->sats_in_view;
        return;
    }
    if (r->kind == NMEA_RMC && r->valid) {
        s_fix.lat = r->lat; s_fix.lon = r->lon; s_fix.ts = r->ts; s_fix.valid = true;
        s_fix.speed = r->speed;
        if (r->has_track) { s_fix.course = r->track; s_fix.has_course = true; }
        s_last_fix_us = esp_timer_get_time();
    } else if (r->kind == NMEA_GGA && r->valid) {
        s_fix.lat = r->lat; s_fix.lon = r->lon;
        s_fix.alt = r->alt; s_fix.sats = r->sats; s_fix.valid = true;
        s_fix.quality = r->quality; s_fix.hdop = r->hdop;
        s_last_fix_us = esp_timer_get_time();
    }
}

static void gps_task(void *arg)
{
    (void)arg;
    uint8_t buf[256];
    while (s_run) {
        int n = uart_read_bytes(GPS_UART_NUM, buf, sizeof buf, pdMS_TO_TICKS(200));
        if (n > 0) {
            s_bytes += (uint32_t)n;
            s_last_data_us = esp_timer_get_time();
            nmea_parser_feed(&s_parser, (const char *)buf, n, on_sentence, NULL);
        }
    }
    vTaskDelete(NULL);
}

esp_err_t gps_init(int rx_pin, int tx_pin, int baud)
{
    nmea_parser_init(&s_parser);
    memset(&s_fix, 0, sizeof s_fix);
    s_bytes = s_sentences = 0;
    s_last_data_us = s_last_fix_us = 0;

    uart_config_t cfg = {
        .baud_rate = baud > 0 ? baud : GPS_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t e = uart_driver_install(GPS_UART_NUM, 1024, 0, 0, NULL, 0);
    if (e != ESP_OK) return e;
    uart_param_config(GPS_UART_NUM, &cfg);
    uart_set_pin(GPS_UART_NUM, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    s_run = true;
    xTaskCreatePinnedToCore(gps_task, "gps", 3072, NULL, 3, NULL, 1);
    ESP_LOGI(TAG, "GPS UART%d rx=%d tx=%d @%d", GPS_UART_NUM, rx_pin, tx_pin, cfg.baud_rate);
    return ESP_OK;
}

void gps_stop(void) { s_run = false; }

bool gps_get_fix(gps_fix_t *out) { *out = s_fix; return s_fix.valid; }

void gps_get_status(gps_status_t *out)
{
    int64_t now = esp_timer_get_time();
    out->running = s_run;
    out->bytes = s_bytes;
    out->sentences = s_sentences;
    out->ms_since_data = s_last_data_us ? (uint32_t)((now - s_last_data_us) / 1000) : UINT32_MAX;
    out->ms_since_fix  = s_last_fix_us  ? (uint32_t)((now - s_last_fix_us)  / 1000) : UINT32_MAX;
    out->fix = s_fix;
}
