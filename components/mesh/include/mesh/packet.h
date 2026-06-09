/* Meshtastic MeshPacket on-air header + full packet build/parse (crypto layer).
 *
 * Ported from firmware/badge/net/mesh/packet.py (host-tested). Header is 16
 * bytes, little-endian:
 *   to u32 | from u32 | id u32 | flags u8 | channel u8 | next_hop u8 | relay_node u8
 * flags: hop_limit = f & 0x07, want_ack = f & 0x08, via_mqtt = f & 0x10,
 *        hop_start = (f & 0xE0) >> 5
 * Payload after the header = AES-CTR(Data protobuf). This module handles the
 * header + crypto only; protobuf encode/decode of the Data payload lives in
 * meshtastic_pb.{c,h}.
 */
#ifndef MESH_PACKET_H
#define MESH_PACKET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MESH_BROADCAST   0xFFFFFFFFu
#define MESH_HEADER_LEN  16

typedef struct {
    uint32_t to;
    uint32_t from;
    uint32_t id;
    uint8_t flags;
    uint8_t hop_limit;
    uint8_t hop_start;
    bool    want_ack;
    bool    via_mqtt;
    uint8_t channel;     /* channel-hash hint */
    uint8_t next_hop;
    uint8_t relay_node;
} mesh_header_t;

uint8_t mesh_build_flags(int hop_limit, int hop_start, bool want_ack, bool via_mqtt);

void mesh_build_header(uint8_t out[16], uint32_t to, uint32_t from, uint32_t id,
                       int hop_limit, int hop_start, bool want_ack, bool via_mqtt,
                       uint8_t ch_hash, uint8_t next_hop, uint8_t relay_node);

void mesh_parse_header(const uint8_t in[16], mesh_header_t *h);

/* Build a full packet (header || AES-CTR(data)) into out. relay_node is set to
 * the low byte of `from`. Returns the total length (16 + data_len), or -1 on a
 * bad PSK / insufficient out_cap. */
int mesh_build_packet(uint8_t *out, size_t out_cap,
                      uint32_t to, uint32_t from, uint32_t id,
                      const char *ch_name, const uint8_t *psk_field, size_t psk_n,
                      const uint8_t *data, size_t data_len,
                      int hop_limit, int hop_start, bool want_ack);

/* Parse + decrypt. Fills hdr and writes the decrypted Data protobuf bytes to
 * plain. Returns:
 *    0  on success (*plain_len set)
 *   -1  malformed input (NULL / too short / bad PSK)
 *   -2  frame is for a different channel (hash mismatch) — silently ignore
 *   -3  plain_cap too small
 * Note: this does not interpret the protobuf; the caller decodes Data and may
 * reject e.g. portnum 0 with an empty payload. */
int mesh_parse_packet(const uint8_t *frame, size_t len,
                      const char *ch_name, const uint8_t *psk_field, size_t psk_n,
                      mesh_header_t *hdr, uint8_t *plain, size_t plain_cap, size_t *plain_len);

/* Decrement the hop_limit in-place for rebroadcast and stamp relay_node.
 * Returns the new hop_limit, or -1 if len < 16. A return of 0 means the packet
 * is exhausted and must not be rebroadcast. */
int mesh_decrement_hop(uint8_t *frame, size_t len, uint8_t relay_node);

#endif /* MESH_PACKET_H */
