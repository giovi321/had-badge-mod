/* See drivers/wifi.h. */
#include "drivers/wifi.h"

#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"

static const char *TAG = "wifi";

static esp_netif_t *s_sta_netif, *s_ap_netif;
static bool s_inited;
static bool s_started;
static char s_mode[8] = "off";
static volatile bool s_got_ip;
static char s_ip[16] = "0.0.0.0";

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_got_ip = false;
        if (strcmp(s_mode, "sta") == 0) esp_wifi_connect();   /* keep retrying */
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        esp_ip4addr_ntoa(&e->ip_info.ip, s_ip, sizeof s_ip);
        s_got_ip = true;
        ESP_LOGI(TAG, "sta got ip %s", s_ip);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_START) {
        esp_netif_ip_info_t ip;
        if (s_ap_netif && esp_netif_get_ip_info(s_ap_netif, &ip) == ESP_OK)
            esp_ip4addr_ntoa(&ip.ip, s_ip, sizeof s_ip);
        s_got_ip = true;
        ESP_LOGI(TAG, "ap started, ip %s", s_ip);
    }
}

void wifi_init(void)
{
    if (s_inited) return;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    /* Move the WiFi TX buffers out of internal DMA RAM: static TX buffers live in
     * internal RAM (~25 KB here), which starves the display's internal draw buffers
     * and the UI task's 8 KB stack and leaves the badge with a white screen. Dynamic
     * TX buffers are allocated from PSRAM (CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP), so
     * this hands that internal RAM back to the display + UI. */
    cfg.tx_buf_type = 1;            /* dynamic (PSRAM) instead of static (internal) */
    cfg.static_tx_buf_num = 0;
    cfg.dynamic_tx_buf_num = 32;
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi, NULL, NULL);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    s_inited = true;
}

esp_err_t wifi_apply(const char *mode, const char *sta_ssid, const char *sta_pass,
                     const char *ap_ssid, const char *ap_pass)
{
    if (!s_inited) wifi_init();
    if (s_started) { esp_wifi_stop(); s_started = false; }
    s_got_ip = false;
    strncpy(s_mode, mode ? mode : "off", sizeof s_mode - 1);
    s_mode[sizeof s_mode - 1] = 0;

    if (strcmp(s_mode, "sta") == 0) {
        wifi_config_t wc = {0};
        strncpy((char *)wc.sta.ssid, sta_ssid ? sta_ssid : "", sizeof wc.sta.ssid - 1);
        strncpy((char *)wc.sta.password, sta_pass ? sta_pass : "", sizeof wc.sta.password - 1);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
        ESP_ERROR_CHECK(esp_wifi_start());
        s_started = true;
        ESP_LOGI(TAG, "sta -> %s", sta_ssid ? sta_ssid : "");
    } else if (strcmp(s_mode, "ap") == 0) {
        wifi_config_t wc = {0};
        const char *ssid = (ap_ssid && ap_ssid[0]) ? ap_ssid : "Communicator";
        strncpy((char *)wc.ap.ssid, ssid, sizeof wc.ap.ssid - 1);
        wc.ap.ssid_len = strlen((char *)wc.ap.ssid);
        wc.ap.max_connection = 4;
        wc.ap.channel = 6;
        if (ap_pass && strlen(ap_pass) >= 8) {
            strncpy((char *)wc.ap.password, ap_pass, sizeof wc.ap.password - 1);
            wc.ap.authmode = WIFI_AUTH_WPA2_PSK;
        } else {
            wc.ap.authmode = WIFI_AUTH_OPEN;
        }
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wc));
        ESP_ERROR_CHECK(esp_wifi_start());
        s_started = true;
        ESP_LOGI(TAG, "ap -> %s (%s)", ssid, wc.ap.authmode == WIFI_AUTH_OPEN ? "open" : "wpa2");
    } else {
        strcpy(s_mode, "off");
        ESP_LOGI(TAG, "off");
    }

    /* Keep the WiFi modem clock steady. The default WIFI_PS_MIN_MODEM power-save,
     * combined with DFS (CONFIG_PM_ENABLE), stalls the LCD SPI flush completion the
     * moment STA associates and blanks the display while the rest of the system keeps
     * running. Disabling power-save costs a little idle current, which is fine since
     * WiFi is used on USB power. */
    if (s_started) esp_wifi_set_ps(WIFI_PS_NONE);
    return ESP_OK;
}

void wifi_get_state(char *buf, int cap, int *rssi, bool *rssi_valid)
{
    if (rssi_valid) *rssi_valid = false;
    if (strcmp(s_mode, "ap") == 0) {
        strncpy(buf, "ap", cap);
    } else if (strcmp(s_mode, "sta") == 0) {
        if (s_got_ip) {
            strncpy(buf, "conn", cap);
            wifi_ap_record_t ap;
            if (rssi && esp_wifi_sta_get_ap_info(&ap) == ESP_OK) { *rssi = ap.rssi; if (rssi_valid) *rssi_valid = true; }
        } else {
            strncpy(buf, "sta", cap);
        }
    } else {
        strncpy(buf, "off", cap);
    }
    buf[cap - 1] = 0;
}

bool wifi_link_up(void) { return s_got_ip; }
void wifi_ip_str(char *buf, int cap) { strncpy(buf, s_ip, cap); buf[cap - 1] = 0; }
