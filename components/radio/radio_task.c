/* See radio/radio_task.h. */
#include "radio/radio_task.h"
#include "drivers/radio.h"
#include "net/backend.h"

#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "radio_task";

typedef struct { uint8_t buf[256]; uint16_t len; } tx_item_t;
static QueueHandle_t s_txq;

bool radio_tx_submit(const uint8_t *frame, int len)
{
    if (!s_txq || len <= 0 || len > 256) return false;
    tx_item_t it;
    memcpy(it.buf, frame, (size_t)len);
    it.len = (uint16_t)len;
    if (xQueueSend(s_txq, &it, 0) != pdTRUE) return false;
    radio_chip_wake();   /* wake the task out of its RX wait */
    return true;
}

static bool drain_tx(void)
{
    tx_item_t it;
    bool sent = false;
    while (xQueueReceive(s_txq, &it, 0) == pdTRUE) {
        sent = true;
        /* Listen-before-talk: bounded CAD with capped backoff (no tight spin). */
        for (int a = 0; a < 16; a++) {
            int c = radio_chip_cad();
            if (c != 1) break;                 /* free or error -> go */
            vTaskDelay(pdMS_TO_TICKS(5 + (esp_random() % 15)));
        }
        if (radio_chip_transmit(it.buf, it.len) != ESP_OK)
            ESP_LOGW(TAG, "tx timeout");
    }
    return sent;
}

static void radio_task(void *arg)
{
    (void)arg;
    uint8_t buf[256];
    radio_chip_start_rx();
    for (;;) {
        /* Only re-arm RX after a transmit (which leaves the chip in standby).
         * Continuous RX stays live across received packets and timeouts, so
         * re-arming every loop would abort an in-progress reception. */
        if (drain_tx()) radio_chip_start_rx();
        if (radio_chip_wait_event(500)) {
            float rssi = 0, snr = 0;
            int n = radio_chip_read_packet(buf, sizeof buf, &rssi, &snr);
            if (n > 0) net_on_frame(buf, n, rssi, snr, (uint32_t)time(NULL));
            else vTaskDelay(1);   /* spurious wake / DIO1 noise: yield, never spin a core */
        }
    }
}

esp_err_t radio_task_start(void)
{
    net_radio_cfg_t c;
    net_radio_cfg(&c);
    radio_params_t p = {
        .freq_mhz = c.freq_mhz, .bw_khz = c.bw_khz, .sf = c.sf, .cr = c.cr,
        .sync_word = c.sync_word, .preamble = c.preamble, .power_dbm = c.power_dbm,
    };
    esp_err_t e = radio_chip_init(&p);
    if (e != ESP_OK) { ESP_LOGE(TAG, "radio init failed: %s", esp_err_to_name(e)); return e; }

    s_txq = xQueueCreate(8, sizeof(tx_item_t));
    net_set_tx(radio_tx_submit);
    xTaskCreatePinnedToCore(radio_task, "radio", 4096, NULL, 6, NULL, 0);
    ESP_LOGI(TAG, "radio task up");
    return ESP_OK;
}
