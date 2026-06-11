/* Multi-channel decode (mesh/channels): a frame is decoded by the channel whose
 * hash+PSK match, and rejected when no configured channel matches. */
#include "test_util.h"
#include "mesh/channels.h"
#include "mesh/mesh_crypto.h"
#include <string.h>

void run_channels(void)
{
    mesh_chan_t ch[2];
    memset(ch, 0, sizeof ch);
    strcpy(ch[0].name, "alpha"); ch[0].psk[0] = 0x01; ch[0].psk_len = 1;
    strcpy(ch[1].name, "bravo"); ch[1].psk[0] = 0x02; ch[1].psk_len = 1;

    int ha = mesh_channel_hash("alpha", ch[0].psk, ch[0].psk_len);
    int hb = mesh_channel_hash("bravo", ch[1].psk, ch[1].psk_len);
    CHECK(ha >= 0 && hb >= 0);
    CHECK(ha != hb);                 /* distinct channels must hash differently */

    const uint8_t data[] = {0x08, 0x01, 0x12, 0x03, 'h', 'i', '!'};
    uint8_t fa[256], fb[256];
    int la = mesh_build_packet(fa, sizeof fa, 0xFFFFFFFFu, 0x1234, 0x55,
                               "alpha", ch[0].psk, ch[0].psk_len, data, sizeof data, 3, 3, false);
    int lb = mesh_build_packet(fb, sizeof fb, 0xFFFFFFFFu, 0x1234, 0x56,
                               "bravo", ch[1].psk, ch[1].psk_len, data, sizeof data, 3, 3, false);
    CHECK(la > 0 && lb > 0);

    mesh_header_t h;
    uint8_t plain[256];
    size_t pl = 0;

    SUITE("channels/resolve");
    CHECK_EQI(mesh_channels_decode(ch, 2, fa, la, &h, plain, sizeof plain, &pl), 0);
    CHECK_EQI((int)pl, (int)sizeof data);
    CHECK_MEM(plain, data, sizeof data);
    CHECK_EQI(mesh_channels_decode(ch, 2, fb, lb, &h, plain, sizeof plain, &pl), 1);

    SUITE("channels/single-channel-list");
    CHECK_EQI(mesh_channels_decode(ch, 1, fb, lb, &h, plain, sizeof plain, &pl), -1);  /* bravo not in [alpha] */
    CHECK_EQI(mesh_channels_decode(ch, 1, fa, la, &h, plain, sizeof plain, &pl), 0);

    SUITE("channels/unknown");
    uint8_t fc[256];
    memcpy(fc, fa, la);
    uint8_t bad = 0;
    while (bad == (uint8_t)ha || bad == (uint8_t)hb) bad++;
    fc[13] = bad;                    /* header byte 13 = channel hash */
    CHECK_EQI(mesh_channels_decode(ch, 2, fc, la, &h, plain, sizeof plain, &pl), -1);
}
