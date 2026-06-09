/* Region / preset / frequency-slot KATs (mirror of firmware/tests/test_regions.py). */
#include "test_util.h"
#include "mesh/regions.h"

void run_regions(void)
{
    SUITE("regions/djb2");
    CHECK_EQI(mesh_djb2("LongFast"), 130429955u);

    SUITE("regions/num_channels");
    const mesh_region_t *us = mesh_region_find("US");
    const mesh_region_t *eu = mesh_region_find("EU_868");
    CHECK(us != NULL);
    CHECK(eu != NULL);
    CHECK_EQI(mesh_num_channels(us, 250.0), 104);
    CHECK_EQI(mesh_num_channels(eu, 250.0), 1);

    SUITE("regions/center_freq");
    CHECK_NEAR(mesh_center_freq("LongFast", eu, 250.0), 869.525, 1e-6);
    CHECK_EQI(mesh_slot_for_channel("LongFast", us, 250.0), 19);
    CHECK_NEAR(mesh_center_freq("LongFast", us, 250.0), 906.875, 1e-6);

    SUITE("regions/presets");
    const mesh_preset_t *lf = mesh_preset_find("LongFast");
    CHECK(lf != NULL);
    CHECK_NEAR(lf->bw_khz, 250.0, 1e-9);
    CHECK_EQI(lf->sf, 11);
    CHECK_EQI(lf->cr, 5);
    CHECK_EQI(lf->preamble, 16);
    CHECK_EQI(MESH_SYNC_WORD, 0x2B);

    SUITE("regions/defaults");
    CHECK_STR(MESH_DEFAULT_REGION, "EU_868");
    CHECK_STR(MESH_DEFAULT_PRESET, "LongFast");
}
