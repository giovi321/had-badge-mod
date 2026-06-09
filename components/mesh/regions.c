/* See mesh/regions.h. */
#include "mesh/regions.h"
#include <string.h>

const char *const MESH_DEFAULT_REGION = "EU_868";
const char *const MESH_DEFAULT_PRESET = "LongFast";

static const mesh_region_t REGIONS[] = {
    {"US",     902.0, 928.0,  30},
    {"EU_868", 869.4, 869.65, 16},
    {"EU_433", 433.0, 434.0,  12},
    {"ANZ",    915.0, 928.0,  30},
    {"AU_915", 915.0, 928.0,  30},
    {"IN",     865.0, 867.0,  30},
    {"JP",     920.8, 927.8,  16},
    {"KR",     920.0, 923.0,   0},
    {"CN",     470.0, 510.0,  19},
    {"RU",     868.7, 869.2,  20},
    {"TW",     920.0, 925.0,  27},
    {"TH",     920.0, 925.0,  16},
    {"UA_868", 868.0, 868.6,  14},
};

/* bw_khz, sf, cr, preamble */
static const mesh_preset_t PRESETS[] = {
    {"ShortTurbo",   500.0,  7, 5, 16},
    {"ShortFast",    250.0,  7, 5, 16},
    {"ShortSlow",    250.0,  8, 5, 16},
    {"MediumFast",   250.0,  9, 5, 16},
    {"MediumSlow",   250.0, 10, 5, 16},
    {"LongFast",     250.0, 11, 5, 16},
    {"LongModerate", 125.0, 11, 8, 16},
    {"LongSlow",     125.0, 12, 8, 16},
    {"VeryLongSlow",  62.5, 12, 8, 16},
};

const mesh_region_t *mesh_region_find(const char *name)
{
    for (size_t i = 0; i < sizeof(REGIONS) / sizeof(REGIONS[0]); i++)
        if (strcmp(REGIONS[i].name, name) == 0) return &REGIONS[i];
    return NULL;
}

const mesh_preset_t *mesh_preset_find(const char *name)
{
    for (size_t i = 0; i < sizeof(PRESETS) / sizeof(PRESETS[0]); i++)
        if (strcmp(PRESETS[i].name, name) == 0) return &PRESETS[i];
    return NULL;
}

uint32_t mesh_djb2(const char *name)
{
    uint32_t h = 5381;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++)
        h = (h << 5) + h + *p; /* h*33 + c, wraps mod 2^32 */
    return h;
}

int mesh_num_channels(const mesh_region_t *region, double bw_khz)
{
    if (!region) return 0;
    double spacing = bw_khz / 1000.0; /* MHz */
    return (int)((region->freq_end - region->freq_start) / spacing + 1e-6);
}

int mesh_slot_for_channel(const char *channel_name, const mesh_region_t *region, double bw_khz)
{
    int n = mesh_num_channels(region, bw_khz);
    if (n <= 0) return 0;
    return (int)(mesh_djb2(channel_name) % (uint32_t)n);
}

double mesh_center_freq(const char *channel_name, const mesh_region_t *region, double bw_khz)
{
    if (!region) return 0.0;
    int slot = mesh_slot_for_channel(channel_name, region, bw_khz);
    return region->freq_start + (bw_khz / 2000.0) + slot * (bw_khz / 1000.0);
}
