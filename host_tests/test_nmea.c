/* NMEA parser KATs (mirror of firmware/tests/test_nmea.py). */
#include "test_util.h"
#include "util/nmea.h"

#define RMC_BODY "GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A"
#define GGA_BODY "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"

static nmea_kind_t g_kinds[8];
static int g_nkinds;
static void collect(const nmea_result_t *r, void *ctx) { (void)ctx; if (g_nkinds < 8) g_kinds[g_nkinds++] = r->kind; }

void run_nmea(void)
{
    nmea_result_t r;

    SUITE("nmea/rmc");
    CHECK(nmea_parse_sentence(RMC_BODY, &r));
    CHECK_EQI(r.kind, NMEA_RMC);
    CHECK(r.valid);
    CHECK_NEAR(r.lat, 48.1173, 1e-3);
    CHECK_NEAR(r.lon, 11.51667, 1e-3);
    CHECK(r.has_datetime);
    CHECK_EQI(r.year, 2094); /* 2-digit year -> 2000+yy */
    CHECK_EQI(r.mon, 3);
    CHECK_EQI(r.day, 23);
    CHECK_EQI(r.hh, 12);
    CHECK_EQI(r.mm, 35);
    CHECK_EQI(r.ss, 19);
    CHECK(r.ts > 0);

    SUITE("nmea/gga");
    CHECK(nmea_parse_sentence(GGA_BODY, &r));
    CHECK_EQI(r.kind, NMEA_GGA);
    CHECK(r.valid);
    CHECK_EQI(r.alt, 545);
    CHECK_EQI(r.sats, 8);

    SUITE("nmea/to-unix");
    CHECK_EQI(nmea_to_unix("010124", "000000"), 1704067200u); /* 2024-01-01T00:00:00Z */

    SUITE("nmea/bad-checksum");
    CHECK(!nmea_parse_sentence("GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*00", &r));

    SUITE("nmea/dm-to-deg");
    double d;
    CHECK(nmea_dm_to_deg("4807.038", 'S', &d) && d < 0);
    CHECK(nmea_dm_to_deg("01131.000", 'W', &d) && d < 0);
    CHECK(!nmea_dm_to_deg("", 'N', &d));

    SUITE("nmea/streaming");
    nmea_parser_t p;
    nmea_parser_init(&p);
    g_nkinds = 0;
    const char *full = "$" RMC_BODY "\r\n$" GGA_BODY "\r\n";
    int len = (int)strlen(full);
    CHECK_EQI(nmea_parser_feed(&p, full, 20, collect, NULL), 0); /* partial line */
    nmea_parser_feed(&p, full + 20, len - 20, collect, NULL);
    CHECK_EQI(g_nkinds, 2);
    CHECK_EQI(g_kinds[0], NMEA_RMC);
    CHECK_EQI(g_kinds[1], NMEA_GGA);
}
