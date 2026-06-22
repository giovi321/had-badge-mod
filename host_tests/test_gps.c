/* gps_state_from() state-machine KATs. */
#include "test_util.h"
#include "util/gps_state.h"

void run_gps(void)
{
    SUITE("gps/off");
    /* Not running -> OFF regardless of the other inputs. */
    CHECK_EQI(gps_state_from(false, 0, 0, true), GPS_STATE_OFF);
    CHECK_EQI(gps_state_from(false, UINT32_MAX, UINT32_MAX, false), GPS_STATE_OFF);

    SUITE("gps/no-data");
    /* Running but no bytes ever, or data gone stale -> NO_DATA (beats a stale fix). */
    CHECK_EQI(gps_state_from(true, UINT32_MAX, UINT32_MAX, false), GPS_STATE_NO_DATA);
    CHECK_EQI(gps_state_from(true, GPS_NO_DATA_MS + 1, 0, true), GPS_STATE_NO_DATA);

    SUITE("gps/searching");
    /* Fresh data but no fresh valid fix -> SEARCHING. */
    CHECK_EQI(gps_state_from(true, 100, UINT32_MAX, false), GPS_STATE_SEARCHING);
    CHECK_EQI(gps_state_from(true, 0, 0, false), GPS_STATE_SEARCHING);
    CHECK_EQI(gps_state_from(true, 0, GPS_FIX_STALE_MS + 1, true), GPS_STATE_SEARCHING);

    SUITE("gps/fix");
    /* Fresh data + fresh valid fix -> FIX, including the threshold boundaries. */
    CHECK_EQI(gps_state_from(true, 0, 0, true), GPS_STATE_FIX);
    CHECK_EQI(gps_state_from(true, GPS_NO_DATA_MS, GPS_FIX_STALE_MS, true), GPS_STATE_FIX);
}
