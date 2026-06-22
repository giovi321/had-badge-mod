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
    int sats;          /* satellites used in the fix (GGA) */
    uint32_t ts;       /* unix seconds, 0 if unknown */
    bool has_course;
    double course;     /* degrees true, course over ground (RMC) */
    double speed;      /* knots */
    int quality;       /* GGA fix quality (0 none, 1 GPS, 2 DGPS, ...) */
    double hdop;       /* horizontal dilution of precision (GGA) */
    int sats_in_view;  /* satellites in view (GSV) */
} gps_fix_t;

/* Health/activity snapshot for diagnostics. Distinguishes "disabled" from
 * "running but no data" from "searching" without the UI having to guess. */
typedef struct {
    bool running;          /* GPS driver task is active (enabled in Settings) */
    uint32_t bytes;        /* total UART bytes received since boot */
    uint32_t sentences;    /* valid NMEA sentences parsed */
    uint32_t ms_since_data;/* since last UART byte; UINT32_MAX if never */
    uint32_t ms_since_fix; /* since last valid fix; UINT32_MAX if never */
    gps_fix_t fix;         /* latest fix snapshot */
} gps_status_t;

esp_err_t gps_init(int rx_pin, int tx_pin, int baud);
void gps_stop(void);

/* Latest fix snapshot. Returns true if a valid fix has been seen. */
bool gps_get_fix(gps_fix_t *out);

/* Full status snapshot (always succeeds; reports running=false when GPS is off). */
void gps_get_status(gps_status_t *out);

#endif /* DRIVERS_GPS_H */
