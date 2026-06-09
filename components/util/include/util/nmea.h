/* Minimal NMEA 0183 parser for the ATGM336H (RMC + GGA). Ported from
 * firmware/badge/services/nmea.py. Pure C, host-tested; fed raw UART bytes. */
#ifndef UTIL_NMEA_H
#define UTIL_NMEA_H

#include <stdint.h>
#include <stdbool.h>

typedef enum { NMEA_NONE = 0, NMEA_RMC, NMEA_GGA } nmea_kind_t;

typedef struct {
    nmea_kind_t kind;
    bool valid;
    bool has_lat, has_lon;
    double lat, lon;
    double speed;          /* knots (RMC) */
    bool has_track;
    double track;          /* degrees true (RMC) */
    int32_t alt;           /* metres (GGA) */
    int sats;              /* GGA */
    int quality;           /* GGA fix quality */
    uint32_t ts;           /* unix seconds (RMC) */
    bool has_datetime;
    int year, mon, day, hh, mm, ss;
} nmea_result_t;

/* Parse one sentence body (without the leading '$'). Returns true on a
 * recognised, checksum-valid RMC/GGA sentence. */
bool nmea_parse_sentence(const char *sentence, nmea_result_t *out);

/* Exposed helpers (also unit-tested). */
bool nmea_dm_to_deg(const char *value, char hemi, double *out);
uint32_t nmea_to_unix(const char *date_ddmmyy, const char *time_hhmmss);

/* Streaming parser: feed arbitrary byte chunks; the callback fires once per
 * complete, valid sentence. Returns the number of sentences emitted. */
typedef struct {
    char buf[256];
    int len;
} nmea_parser_t;

typedef void (*nmea_cb_t)(const nmea_result_t *r, void *ctx);

void nmea_parser_init(nmea_parser_t *p);
int nmea_parser_feed(nmea_parser_t *p, const char *data, int n, nmea_cb_t cb, void *ctx);

#endif /* UTIL_NMEA_H */
