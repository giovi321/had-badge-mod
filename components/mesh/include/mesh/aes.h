/* Minimal AES-128/AES-256 block cipher (encrypt only) for CTR mode.
 *
 * Portable C11, no platform dependencies — compiles into the firmware and the
 * host test harness identically. Encrypt-only is sufficient because Meshtastic
 * channel crypto is AES-CTR, which uses the forward cipher for both directions.
 *
 * Validated against the FIPS-197 Appendix C known-answer vectors in the host
 * tests (AES-128 and AES-256).
 */
#ifndef MESH_AES_H
#define MESH_AES_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t round_key[240]; /* (Nr+1)*16, max 15 rounds for AES-256 */
    int nr;                 /* number of rounds: 10 (AES-128) or 14 (AES-256) */
} aes_ctx_t;

/* Initialise the key schedule. key_bits must be 128 or 256. Returns 0 on
 * success, -1 on an unsupported key length. */
int aes_setkey_enc(aes_ctx_t *ctx, const uint8_t *key, int key_bits);

/* Encrypt one 16-byte block in place-free fashion (in and out may alias). */
void aes_encrypt_block(const aes_ctx_t *ctx, const uint8_t in[16], uint8_t out[16]);

#endif /* MESH_AES_H */
