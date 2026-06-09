/* See mesh/packet.h. */
#include "mesh/packet.h"
#include "mesh/mesh_crypto.h"
#include <string.h>

static void put_u32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get_u32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint8_t mesh_build_flags(int hop_limit, int hop_start, bool want_ack, bool via_mqtt)
{
    return (uint8_t)(((hop_limit & 0x07)) |
                     (want_ack ? 0x08 : 0) |
                     (via_mqtt ? 0x10 : 0) |
                     ((hop_start & 0x07) << 5));
}

void mesh_build_header(uint8_t out[16], uint32_t to, uint32_t from, uint32_t id,
                       int hop_limit, int hop_start, bool want_ack, bool via_mqtt,
                       uint8_t ch_hash, uint8_t next_hop, uint8_t relay_node)
{
    put_u32le(out + 0, to);
    put_u32le(out + 4, from);
    put_u32le(out + 8, id);
    out[12] = mesh_build_flags(hop_limit, hop_start, want_ack, via_mqtt);
    out[13] = ch_hash;
    out[14] = next_hop;
    out[15] = relay_node;
}

void mesh_parse_header(const uint8_t in[16], mesh_header_t *h)
{
    h->to   = get_u32le(in + 0);
    h->from = get_u32le(in + 4);
    h->id   = get_u32le(in + 8);
    uint8_t f = in[12];
    h->flags      = f;
    h->hop_limit  = (uint8_t)(f & 0x07);
    h->want_ack   = (f & 0x08) != 0;
    h->via_mqtt   = (f & 0x10) != 0;
    h->hop_start  = (uint8_t)((f & 0xE0) >> 5);
    h->channel    = in[13];
    h->next_hop   = in[14];
    h->relay_node = in[15];
}

int mesh_build_packet(uint8_t *out, size_t out_cap,
                      uint32_t to, uint32_t from, uint32_t id,
                      const char *ch_name, const uint8_t *psk_field, size_t psk_n,
                      const uint8_t *data, size_t data_len,
                      int hop_limit, int hop_start, bool want_ack)
{
    if (out_cap < MESH_HEADER_LEN + data_len) return -1;
    int ch = mesh_channel_hash(ch_name, psk_field, psk_n);
    if (ch < 0) return -1;

    uint8_t key[32];
    int key_len = mesh_expand_psk(psk_field, psk_n, key);
    if (key_len < 0) return -1;

    mesh_build_header(out, to, from, id, hop_limit, hop_start, want_ack, false,
                      (uint8_t)ch, 0, (uint8_t)(from & 0xFF));

    uint8_t nonce[16];
    mesh_build_nonce(id, from, nonce);
    mesh_aes_ctr_xcrypt(key, (size_t)key_len, nonce, data, out + MESH_HEADER_LEN, data_len);
    return (int)(MESH_HEADER_LEN + data_len);
}

int mesh_parse_packet(const uint8_t *frame, size_t len,
                      const char *ch_name, const uint8_t *psk_field, size_t psk_n,
                      mesh_header_t *hdr, uint8_t *plain, size_t plain_cap, size_t *plain_len)
{
    if (frame == NULL || len < MESH_HEADER_LEN) return -1;
    mesh_parse_header(frame, hdr);

    int ch = mesh_channel_hash(ch_name, psk_field, psk_n);
    if (ch < 0) return -1;
    if (hdr->channel != (uint8_t)ch) return -2;

    size_t ct_len = len - MESH_HEADER_LEN;
    if (ct_len > plain_cap) return -3;

    uint8_t key[32];
    int key_len = mesh_expand_psk(psk_field, psk_n, key);
    if (key_len < 0) return -1;

    uint8_t nonce[16];
    mesh_build_nonce(hdr->id, hdr->from, nonce);
    mesh_aes_ctr_xcrypt(key, (size_t)key_len, nonce, frame + MESH_HEADER_LEN, plain, ct_len);
    *plain_len = ct_len;
    return 0;
}

int mesh_decrement_hop(uint8_t *frame, size_t len, uint8_t relay_node)
{
    if (len < MESH_HEADER_LEN) return -1;
    uint8_t flags = frame[12];
    uint8_t hop = (uint8_t)(flags & 0x07);
    if (hop == 0) return 0;
    hop--;
    frame[12] = (uint8_t)((flags & 0xF8) | (hop & 0x07));
    frame[15] = relay_node;
    return hop;
}
