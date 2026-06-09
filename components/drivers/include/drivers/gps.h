/* ATGM336H GPS over UART (NMEA). Optional; only started when enabled. */
#ifndef DRIVERS_GPS_H
#define DRIVERS_GPS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    bool valid;
    double lat, lon;
    int32_t alt;
    int sats;
    uint32_t ts;       /* unix seconds, 0 if unknown */
    bool has_course;
    double course;     /* degrees true, course over ground (RMC) */
    double speed;      /* knots */
} gps_fix_t;

esp_err_t gps_init(int rx_pin, int tx_pin, int baud);
void gps_stop(void);

/* Latest fix snapshot. Returns true if a valid fix has been seen. */
bool gps_get_fix(gps_fix_t *out);

#endif /* DRIVERS_GPS_H */
