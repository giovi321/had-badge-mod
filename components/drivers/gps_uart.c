/* See drivers/gps.h. UART RX -> NMEA parser -> latest fix snapshot. */
#include "drivers/gps.h"
#include "board_pins.h"
#include "util/nmea.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

static const char *TAG = "gps";
static gps_fix_t s_fix;
static volatile bool s_run;
static nmea_parser_t s_parser;

static void on_sentence(const nmea_result_t *r, void *ctx)
{
    (void)ctx;
    if (r->kind == NMEA_RMC && r->valid) {
        s_fix.lat = r->lat; s_fix.lon = r->lon; s_fix.ts = r->ts; s_fix.valid = true;
        s_fix.speed = r->speed;
        if (r->has_track) { s_fix.course = r->track; s_fix.has_course = true; }
    } else if (r->kind == NMEA_GGA && r->valid) {
        s_fix.lat = r->lat; s_fix.lon = r->lon;
        s_fix.alt = r->alt; s_fix.sats = r->sats; s_fix.valid = true;
    }
}

static void gps_task(void *arg)
{
    (void)arg;
    uint8_t buf[256];
    while (s_run) {
        int n = uart_read_bytes(GPS_UART_NUM, buf, sizeof buf, pdMS_TO_TICKS(200));
        if (n > 0) nmea_parser_feed(&s_parser, (const char *)buf, n, on_sentence, NULL);
    }
    vTaskDelete(NULL);
}

esp_err_t gps_init(int rx_pin, int tx_pin, int baud)
{
    nmea_parser_init(&s_parser);
    memset(&s_fix, 0, sizeof s_fix);

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
