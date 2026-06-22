/* See util/gps_state.h. */
#include "util/gps_state.h"

gps_state_t gps_state_from(bool running, uint32_t ms_since_data,
                           uint32_t ms_since_fix, bool has_fix)
{
    if (!running) return GPS_STATE_OFF;
    if (ms_since_data == UINT32_MAX || ms_since_data > GPS_NO_DATA_MS)
        return GPS_STATE_NO_DATA;
    if (has_fix && ms_since_fix <= GPS_FIX_STALE_MS)
        return GPS_STATE_FIX;
    return GPS_STATE_SEARCHING;
}
