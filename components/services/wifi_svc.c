/* WiFi service: registers the WiFi settings, brings the radio up from them, and
 * starts the web UI when enabled. */
#include "services/services.h"
#include "services/web.h"
#include "drivers/wifi.h"

#include <string.h>
#include "esp_log.h"

static const char *TAG = "wifi_svc";
static const char *WIFI_CHOICES[] = {"off", "sta", "ap"};

static const setting_t WIFI_SCHEMA[] = {
    {.key = "wifi_mode", .type = SET_ENUM, .def = "off", .label = "WiFi mode", .group = "WiFi",
     .choices = WIFI_CHOICES, .nchoices = 3},
    {.key = "wifi_ssid", .type = SET_STR, .def = "", .label = "WiFi SSID", .group = "WiFi"},
    {.key = "wifi_pass", .type = SET_STR, .def = "", .label = "WiFi password", .group = "WiFi", .secret = true},
    {.key = "ap_ssid", .type = SET_STR, .def = "Communicator", .label = "Hotspot name", .group = "WiFi"},
    {.key = "ap_pass", .type = SET_STR, .def = "", .label = "Hotspot password", .group = "WiFi", .secret = true},
    {.key = "web_enabled", .type = SET_BOOL, .def = "true", .label = "Web interface", .group = "WiFi"},
};

void wifi_svc_init(settings_t *reg)
{
    settings_register_many(reg, WIFI_SCHEMA, (int)(sizeof WIFI_SCHEMA / sizeof WIFI_SCHEMA[0]));

    char mode[8];
    settings_get_str(reg, "wifi_mode", mode, sizeof mode);
    if (strcmp(mode, "off") == 0) { ESP_LOGI(TAG, "WiFi off"); return; }

    char ssid[64], pass[80], apssid[40], appass[80];
    settings_get_str(reg, "wifi_ssid", ssid, sizeof ssid);
    settings_get_str(reg, "wifi_pass", pass, sizeof pass);
    settings_get_str(reg, "ap_ssid", apssid, sizeof apssid);
    settings_get_str(reg, "ap_pass", appass, sizeof appass);

    wifi_init();
    wifi_apply(mode, ssid, pass, apssid, appass);

    if (settings_get_bool(reg, "web_enabled")) web_svc_start(reg);
}
