/* See mesh/meshtastic_pb.h. */
#include "mesh/meshtastic_pb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include <string.h>

int mt_data_encode(uint8_t *buf, size_t cap, const meshtastic_Data *d)
{
    pb_ostream_t s = pb_ostream_from_buffer(buf, cap);
    if (!pb_encode(&s, meshtastic_Data_fields, d)) return -1;
    return (int)s.bytes_written;
}

int mt_position_encode(uint8_t *buf, size_t cap, const meshtastic_Position *p)
{
    pb_ostream_t s = pb_ostream_from_buffer(buf, cap);
    if (!pb_encode(&s, meshtastic_Position_fields, p)) return -1;
    return (int)s.bytes_written;
}

int mt_user_encode(uint8_t *buf, size_t cap, const meshtastic_User *u)
{
    pb_ostream_t s = pb_ostream_from_buffer(buf, cap);
    if (!pb_encode(&s, meshtastic_User_fields, u)) return -1;
    return (int)s.bytes_written;
}

bool mt_data_decode(const uint8_t *buf, size_t len, meshtastic_Data *d)
{
    *d = (meshtastic_Data)meshtastic_Data_init_zero;
    pb_istream_t s = pb_istream_from_buffer(buf, len);
    return pb_decode(&s, meshtastic_Data_fields, d);
}

bool mt_position_decode(const uint8_t *buf, size_t len, meshtastic_Position *p)
{
    *p = (meshtastic_Position)meshtastic_Position_init_zero;
    pb_istream_t s = pb_istream_from_buffer(buf, len);
    return pb_decode(&s, meshtastic_Position_fields, p);
}

bool mt_user_decode(const uint8_t *buf, size_t len, meshtastic_User *u)
{
    *u = (meshtastic_User)meshtastic_User_init_zero;
    pb_istream_t s = pb_istream_from_buffer(buf, len);
    return pb_decode(&s, meshtastic_User_fields, u);
}

int mt_data_make_text(meshtastic_Data *d, const char *text)
{
    *d = (meshtastic_Data)meshtastic_Data_init_zero;
    d->portnum = meshtastic_PortNum_TEXT_MESSAGE_APP;
    size_t n = strlen(text);
    if (n > sizeof(d->payload.bytes)) return -1;
    memcpy(d->payload.bytes, text, n);
    d->payload.size = (pb_size_t)n;
    return 0;
}
