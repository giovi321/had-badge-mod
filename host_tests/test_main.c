/* Host test runner: links the portable firmware sources and every test suite
 * into one executable. Returns non-zero if any check fails.
 *
 * Suites are declared weak so the harness links and runs whatever subset is
 * present (clang/gcc/zig). The committed tree implements them all. */
#include "test_util.h"

int ht_checks = 0;
int ht_fails = 0;
const char *ht_cur = "";

#define SUITE_DECL(f) void f(void) __attribute__((weak))
SUITE_DECL(run_aes);
SUITE_DECL(run_mesh_crypto);
SUITE_DECL(run_regions);
SUITE_DECL(run_packet);
SUITE_DECL(run_channels);
SUITE_DECL(run_meshtastic_pb);
SUITE_DECL(run_dedup);
SUITE_DECL(run_ack_queue);
SUITE_DECL(run_eventbus);
SUITE_DECL(run_settings);
SUITE_DECL(run_settings_json);
SUITE_DECL(run_nodedb);
SUITE_DECL(run_nmea);
SUITE_DECL(run_geo);
SUITE_DECL(run_radar);
SUITE_DECL(run_theme_layout);

#define RUN(f) do { if (f) (f)(); } while (0)

int main(void)
{
    RUN(run_aes);
    RUN(run_mesh_crypto);
    RUN(run_regions);
    RUN(run_packet);
    RUN(run_channels);
    RUN(run_meshtastic_pb);
    RUN(run_dedup);
    RUN(run_ack_queue);
    RUN(run_eventbus);
    RUN(run_settings);
    RUN(run_settings_json);
    RUN(run_nodedb);
    RUN(run_nmea);
    RUN(run_geo);
    RUN(run_radar);
    RUN(run_theme_layout);

    printf("\n%d checks, %d failure%s\n", ht_checks, ht_fails, ht_fails == 1 ? "" : "s");
    return ht_fails ? 1 : 0;
}
