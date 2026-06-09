/* See mesh/mesh_crypto.h. */
#include "mesh/mesh_crypto.h"
#include "mesh/aes.h"
#include <string.h>

const uint8_t MESH_DEFAULT_PSK[16] = {
    0xD4, 0xF1, 0xBB, 0x3A, 0x20, 0x29, 0x07, 0x59,
    0xF0, 0xBC, 0xFF, 0xAB, 0xCF, 0x4E, 0x69, 0x01
};

int mesh_expand_psk(const uint8_t *field, size_t n, uint8_t out[32])
{
    if (n == 0) return 0;
    if (n == 1) {
        if (field[0] == 0) return 0;
        memcpy(out, MESH_DEFAULT_PSK, 16);
        out[15] = (uint8_t)(out[15] + field[0] - 1);
        return 16;
    }
    if (n == 16 || n == 32) {
        memcpy(out, field, n);
        return (int)n;
    }
    return -1;
}

uint8_t mesh_xor_hash(const uint8_t *data, size_t n)
{
    uint8_t h = 0;
    for (size_t i = 0; i < n; i++) h ^= data[i];
    return h;
}

int mesh_channel_hash(const char *name, const uint8_t *psk_field, size_t psk_n)
{
    uint8_t key[32];
    int key_len = mesh_expand_psk(psk_field, psk_n, key);
    if (key_len < 0) return -1;
    uint8_t h = mesh_xor_hash((const uint8_t *)name, strlen(name)) ^
                mesh_xor_hash(key, (size_t)key_len);
    return h;
}

void mesh_build_nonce(uint32_t packet_id, uint32_t from_node, uint8_t nonce[16])
{
    memset(nonce, 0, 16);
    nonce[0] = (uint8_t)(packet_id);
    nonce[1] = (uint8_t)(packet_id >> 8);
    nonce[2] = (uint8_t)(packet_id >> 16);
    nonce[3] = (uint8_t)(packet_id >> 24);
    /* bytes 4..7 stay zero (high 32 bits of the u64 packet id) */
    nonce[8]  = (uint8_t)(from_node);
    nonce[9]  = (uint8_t)(from_node >> 8);
    nonce[10] = (uint8_t)(from_node >> 16);
    nonce[11] = (uint8_t)(from_node >> 24);
    /* bytes 12..15 stay zero (extraNonce, only set for PKI packets) */
}

void mesh_aes_ctr_xcrypt(const uint8_t *key, size_t key_len, const uint8_t nonce[16],
                         const uint8_t *in, uint8_t *out, size_t len)
{
    if (key_len == 0) {
        if (out != in) memmove(out, in, len);
        return;
    }
    aes_ctx_t ctx;
    aes_setkey_enc(&ctx, key, (int)(key_len * 8));

    uint8_t counter[16];
    memcpy(counter, nonce, 16);
    uint8_t ks[16];

    size_t off = 0;
    while (off < len) {
        aes_encrypt_block(&ctx, counter, ks);
        size_t chunk = len - off;
        if (chunk > 16) chunk = 16;
        for (size_t i = 0; i < chunk; i++) out[off + i] = (uint8_t)(in[off + i] ^ ks[i]);
        off += 16;
        /* increment 128-bit big-endian counter */
        for (int i = 15; i >= 0; i--) {
            if (++counter[i] != 0) break;
        }
    }
}

/* --- base64 (decode only, for PSK strings) ----------------------------- */
static int b64_val(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

int mesh_parse_psk_b64(const char *b64, uint8_t out[32])
{
    int acc = 0, bits = 0, n = 0;
    for (const char *p = b64; *p; p++) {
        if (*p == '=' || *p == '\n' || *p == '\r' || *p == ' ') continue;
        int v = b64_val(*p);
        if (v < 0) return -1;
        acc = (acc << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (n >= 32) return -1;
            out[n++] = (uint8_t)((acc >> bits) & 0xFF);
        }
    }
    return n;
}
