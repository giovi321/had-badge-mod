/* A small set of Meshtastic channels sharing one LoRa frequency, separated by
 * the per-channel hash + PSK. Portable and host-tested: given a received frame,
 * find which configured channel decodes it. */
#ifndef MESH_CHANNELS_H
#define MESH_CHANNELS_H

#include "mesh/packet.h"
#include <stdint.h>
#include <stddef.h>

#ifndef MESH_MAX_CHAN
#define MESH_MAX_CHAN 4
#endif

typedef struct {
    char name[24];
    uint8_t psk[32];    /* the stored PSK field (as mesh_parse_psk_b64 returns) */
    size_t psk_len;
} mesh_chan_t;

/* Try to decode `frame` against each of the n channels, in order. On the first
 * channel that matches (header hash) and decrypts, returns its index (0..n-1)
 * and fills hdr + the decrypted Data bytes into plain. Returns -1 if no channel
 * matches. (A matching hash does not validate the protobuf; the caller decodes
 * Data and may still reject it.) */
int mesh_channels_decode(const mesh_chan_t *ch, int n,
                         const uint8_t *frame, size_t len, mesh_header_t *hdr,
                         uint8_t *plain, size_t plain_cap, size_t *plain_len);

#endif /* MESH_CHANNELS_H */
