/* Meshtastic channel crypto: default PSK, PSK expansion, channel hash, AES-CTR.
 *
 * Ported from firmware/badge/net/mesh/mesh_crypto.py (host-tested). Verified
 * Meshtastic facts:
 *   default PSK "AQ==" (1-byte 0x01) -> d4f1bb3a20290759f0bcffabcf4e6901 (AES-128)
 *   shorthand n: copy default key, key[15] += n-1
 *   channel hash byte = xorHash(name) ^ xorHash(expanded_psk)
 *   CTR nonce (16B) = packetId u32 LE (0-3) | 0000 (4-7) | fromNode u32 LE (8-11) | 0000
 *   nonce is the initial big-endian 128-bit counter, +1 per block.
 */
#ifndef MESH_CRYPTO_H
#define MESH_CRYPTO_H

#include <stdint.h>
#include <stddef.h>

extern const uint8_t MESH_DEFAULT_PSK[16];

/* Expand a stored PSK field into the AES key bytes.
 *   n == 0            -> no encryption (returns 0)
 *   n == 1, field 0   -> no encryption (returns 0)
 *   n == 1, field k   -> default key with key[15] += k-1 (returns 16)
 *   n == 16 or 32     -> copied verbatim (returns 16 or 32)
 * Any other length returns -1. out must hold at least 32 bytes. */
int mesh_expand_psk(const uint8_t *field, size_t n, uint8_t out[32]);

/* Decode a base64 PSK string (e.g. "AQ==") into a raw PSK field. Returns the
 * field length, or -1 on malformed input. out must hold at least 32 bytes. */
int mesh_parse_psk_b64(const char *b64, uint8_t out[32]);

uint8_t mesh_xor_hash(const uint8_t *data, size_t n);

/* 1-byte channel hash stored in the packet header (decrypt hint). Returns
 * 0..255, or -1 if the PSK field is malformed. */
int mesh_channel_hash(const char *name, const uint8_t *psk_field, size_t psk_n);

void mesh_build_nonce(uint32_t packet_id, uint32_t from_node, uint8_t nonce[16]);

/* Encrypt or decrypt in place-free fashion (CTR is symmetric). key_len 0 means
 * no encryption (out := in). in and out may alias. */
void mesh_aes_ctr_xcrypt(const uint8_t *key, size_t key_len, const uint8_t nonce[16],
                         const uint8_t *in, uint8_t *out, size_t len);

#endif /* MESH_CRYPTO_H */
