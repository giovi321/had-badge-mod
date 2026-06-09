/* Meshtastic region frequency plans, modem presets, and channel-slot math.
 *
 * Ported from firmware/badge/net/mesh/regions.py (host-tested). Frequencies in
 * MHz, from Meshtastic firmware RegionInfo. Channel slot:
 *   numChannels = floor((end - start) / (bw_kHz/1000))
 *   slot        = djb2(channel_name) % numChannels      (0-based)
 *   center      = start + bw_kHz/2000 + slot*(bw_kHz/1000)
 * Verified KATs: EU_868 LongFast -> 869.525; US LongFast slot 19 -> 906.875.
 */
#ifndef MESH_REGIONS_H
#define MESH_REGIONS_H

#include <stdint.h>

#define MESH_SYNC_WORD 0x2B  /* Meshtastic LoRa sync word (NOT BadgeNet 0x12) */

extern const char *const MESH_DEFAULT_REGION;  /* "EU_868" */
extern const char *const MESH_DEFAULT_PRESET;  /* "LongFast" */

typedef struct {
    const char *name;
    double freq_start;   /* MHz */
    double freq_end;     /* MHz */
    int power_limit_dbm;
} mesh_region_t;

typedef struct {
    const char *name;
    double bw_khz;
    int sf;
    int cr;        /* SX126x coding-rate value: 5..8 == 4/5..4/8 */
    int preamble;
} mesh_preset_t;

/* Lookups return NULL for an unknown name. */
const mesh_region_t *mesh_region_find(const char *name);
const mesh_preset_t *mesh_preset_find(const char *name);

uint32_t mesh_djb2(const char *name);
int mesh_num_channels(const mesh_region_t *region, double bw_khz);
int mesh_slot_for_channel(const char *channel_name, const mesh_region_t *region, double bw_khz);
double mesh_center_freq(const char *channel_name, const mesh_region_t *region, double bw_khz);

#endif /* MESH_REGIONS_H */
