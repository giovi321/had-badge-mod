/* Meshtastic channel-crypto KATs (mirror of firmware/tests/test_mesh_crypto.py). */
#include "test_util.h"
#include "mesh/mesh_crypto.h"

void run_mesh_crypto(void)
{
    uint8_t key[32], out[64];

    SUITE("mesh_crypto/expand_psk");
    /* default PSK from 1-byte shorthand 0x01 */
    CHECK_EQI(mesh_expand_psk((const uint8_t[]){0x01}, 1, key), 16);
    CHECK_MEM(key, MESH_DEFAULT_PSK, 16);
    /* "AQ==" base64 decodes to 0x01 */
    CHECK_EQI(mesh_parse_psk_b64("AQ==", out), 1);
    CHECK_EQI(out[0], 0x01);
    /* shorthand 0x02 bumps key[15] */
    CHECK_EQI(mesh_expand_psk((const uint8_t[]){0x02}, 1, key), 16);
    CHECK_MEM(key, MESH_DEFAULT_PSK, 15);
    CHECK_EQI(key[15], (uint8_t)(MESH_DEFAULT_PSK[15] + 1));
    /* disabled / empty / full / invalid */
    CHECK_EQI(mesh_expand_psk((const uint8_t[]){0x00}, 1, key), 0);
    CHECK_EQI(mesh_expand_psk(NULL, 0, key), 0);
    uint8_t full[16]; for (int i = 0; i < 16; i++) full[i] = (uint8_t)i;
    CHECK_EQI(mesh_expand_psk(full, 16, key), 16);
    CHECK_MEM(key, full, 16);
    CHECK_EQI(mesh_expand_psk((const uint8_t *)"123", 3, key), -1);

    SUITE("mesh_crypto/channel_hash");
    /* canonical Meshtastic default channel hash */
    CHECK_EQI(mesh_channel_hash("LongFast", (const uint8_t[]){0x01}, 1), 0x08);
    CHECK_EQI(mesh_channel_hash("LongFast", MESH_DEFAULT_PSK, 16), 0x08);

    SUITE("mesh_crypto/nonce");
    uint8_t nonce[16];
    mesh_build_nonce(0x11223344u, 0xAABBCCDDu, nonce);
    CHECK_MEM(nonce + 0, "\x44\x33\x22\x11", 4);
    CHECK_MEM(nonce + 4, "\x00\x00\x00\x00", 4);
    CHECK_MEM(nonce + 8, "\xDD\xCC\xBB\xAA", 4);
    CHECK_MEM(nonce + 12, "\x00\x00\x00\x00", 4);

    SUITE("mesh_crypto/ctr-roundtrip");
    const char *pt = "Hello Meshtastic, this is a badge!";
    size_t plen = strlen(pt);
    uint8_t ct[64], back[64];
    mesh_build_nonce(0xDEADBEEFu, 0x12345678u, nonce);
    mesh_aes_ctr_xcrypt(MESH_DEFAULT_PSK, 16, nonce, (const uint8_t *)pt, ct, plen);
    CHECK(memcmp(ct, pt, plen) != 0);
    mesh_aes_ctr_xcrypt(MESH_DEFAULT_PSK, 16, nonce, ct, back, plen);
    CHECK_MEM(back, pt, plen);

    SUITE("mesh_crypto/ctr-aes256");
    uint8_t k256[32]; for (int i = 0; i < 32; i++) k256[i] = (uint8_t)(i * 7 + 1);
    uint8_t big[40]; for (int i = 0; i < 40; i++) big[i] = 'x';
    mesh_build_nonce(1, 2, nonce);
    mesh_aes_ctr_xcrypt(k256, 32, nonce, big, ct, 40);
    mesh_aes_ctr_xcrypt(k256, 32, nonce, ct, back, 40);
    CHECK_MEM(back, big, 40);

    SUITE("mesh_crypto/ctr-disabled");
    mesh_aes_ctr_xcrypt(key, 0, nonce, (const uint8_t *)pt, ct, plen);
    CHECK_MEM(ct, pt, plen); /* no key -> passthrough */
}
