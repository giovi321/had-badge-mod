/* Pure GPS state derivation, shared by the GPS app and Diag pages (and host-tested).
 * No ESP/LVGL deps so it also builds in the host-test harness, like nmea/geo. */
#ifndef UTIL_GPS_STATE_H
#define UTIL_GPS_STATE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    GPS_STATE_OFF,        /* driver not running (GPS disabled in Settings) */
    GPS_STATE_NO_DATA,    /* running, but no NMEA bytes arriving (wiring/pins/module) */
    GPS_STATE_SEARCHING,  /* data flowing, no valid fix yet */
    GPS_STATE_FIX,        /* valid, fresh fix */
} gps_state_t;

/* Thresholds in milliseconds. */
#define GPS_NO_DATA_MS    3000u   /* no bytes for at least this long -> NO_DATA */
#define GPS_FIX_STALE_MS  5000u   /* a fix older than this no longer counts as FIX */

/* Derive the coarse state. ms_since_data / ms_since_fix use UINT32_MAX for "never seen". */
gps_state_t gps_state_from(bool running, uint32_t ms_since_data,
                           uint32_t ms_since_fix, bool has_fix);

#endif /* UTIL_GPS_STATE_H */
