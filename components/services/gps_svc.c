/* GPS service: registers GPS settings and starts the UART driver when enabled. */
#include "services/services.h"
#include "drivers/gps.h"
#include "board_pins.h"
#include "esp_log.h"

static const setting_t GPS_SCHEMA[] = {
    {.key = "gps_enabled", .type = SET_BOOL, .def = "false", .label = "GPS enabled", .group = "GPS"},
    {.key = "gps_rx_pin",  .type = SET_INT,  .def = "12", .label = "GPS RX pin (ESP)", .group = "GPS",
     .minv = 0, .maxv = 48, .has_min = true, .has_max = true},
    {.key = "gps_tx_pin",  .type = SET_INT,  .def = "11", .label = "GPS TX pin (ESP)", .group = "GPS",
     .minv = 0, .maxv = 48, .has_min = true, .has_max = true},
};

void gps_svc_init(settings_t *reg)
{
    settings_register_many(reg, GPS_SCHEMA, (int)(sizeof GPS_SCHEMA / sizeof GPS_SCHEMA[0]));
    if (!settings_get_bool(reg, "gps_enabled")) return;
    int rx = (int)settings_get_int(reg, "gps_rx_pin");
    int tx = (int)settings_get_int(reg, "gps_tx_pin");
    gps_init(rx, tx, GPS_UART_BAUD);
}
