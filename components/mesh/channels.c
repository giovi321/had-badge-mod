/* See mesh/channels.h. */
#include "mesh/channels.h"

int mesh_channels_decode(const mesh_chan_t *ch, int n,
                         const uint8_t *frame, size_t len, mesh_header_t *hdr,
                         uint8_t *plain, size_t plain_cap, size_t *plain_len)
{
    for (int i = 0; i < n; i++)
        if (mesh_parse_packet(frame, len, ch[i].name, ch[i].psk, ch[i].psk_len,
                              hdr, plain, plain_cap, plain_len) == 0)
            return i;
    return -1;
}
