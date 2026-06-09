/* Meshtastic protobuf encode/decode via nanopb (mirror of test_pb_messages.py)
 * plus a wire-format KAT proving nanopb matches the hand-rolled Python codec,
 * and an end-to-end packet+protobuf roundtrip. */
#include "test_util.h"
#include "mesh/meshtastic_pb.h"
#include "mesh/packet.h"

void run_meshtastic_pb(void)
{
    uint8_t buf[300];
    int n;

    SUITE("pb/data-wire-format");
    /* encode_data(TEXT, "hi") must be exactly: 08 01 12 02 68 69
     * (field1 varint=1 ; field2 bytes len=2 "hi") — identical to the Python codec. */
    meshtastic_Data d;
    CHECK_EQI(mt_data_make_text(&d, "hi"), 0);
    n = mt_data_encode(buf, sizeof buf, &d);
    CHECK_EQI(n, 6);
    CHECK_MEM(buf, "\x08\x01\x12\x02\x68\x69", 6);

    SUITE("pb/data-roundtrip-text");
    CHECK_EQI(mt_data_make_text(&d, "hello world"), 0);
    n = mt_data_encode(buf, sizeof buf, &d);
    CHECK(n > 0);
    meshtastic_Data d2;
    CHECK(mt_data_decode(buf, (size_t)n, &d2));
    CHECK_EQI(d2.portnum, meshtastic_PortNum_TEXT_MESSAGE_APP);
    CHECK_EQI(d2.payload.size, 11);
    CHECK_MEM(d2.payload.bytes, "hello world", 11);

    SUITE("pb/data-optional-fields");
    d = (meshtastic_Data)meshtastic_Data_init_zero;
    d.portnum = meshtastic_PortNum_POSITION_APP;
    d.payload.size = 2; d.payload.bytes[0] = 0x01; d.payload.bytes[1] = 0x02;
    d.want_response = true;
    d.dest = 0xFFFFFFFFu; d.source = 0x12345678u; d.request_id = 0xABCDEF01u;
    n = mt_data_encode(buf, sizeof buf, &d);
    CHECK(mt_data_decode(buf, (size_t)n, &d2));
    CHECK_EQI(d2.portnum, meshtastic_PortNum_POSITION_APP);
    CHECK_EQI(d2.payload.size, 2);
    CHECK(d2.want_response);
    CHECK_EQI(d2.dest, 0xFFFFFFFFu);
    CHECK_EQI(d2.source, 0x12345678u);
    CHECK_EQI(d2.request_id, 0xABCDEF01u);

    SUITE("pb/position-roundtrip");
    meshtastic_Position p = meshtastic_Position_init_zero;
    p.latitude_i = 458566000; p.longitude_i = 93976000; /* 45.8566, 9.3976 */
    p.altitude = 214; p.time = 1750000000u; p.sats_in_view = 9;
    n = mt_position_encode(buf, sizeof buf, &p);
    CHECK(n > 0);
    meshtastic_Position p2;
    CHECK(mt_position_decode(buf, (size_t)n, &p2));
    CHECK_EQI(p2.latitude_i, 458566000);
    CHECK_EQI(p2.longitude_i, 93976000);
    CHECK_EQI(p2.altitude, 214);
    CHECK_EQI(p2.time, 1750000000u);
    CHECK_EQI(p2.sats_in_view, 9);

    SUITE("pb/position-negative");
    p = (meshtastic_Position)meshtastic_Position_init_zero;
    p.latitude_i = -338688000; p.longitude_i = 1512093000; p.altitude = -12;
    n = mt_position_encode(buf, sizeof buf, &p);
    CHECK(mt_position_decode(buf, (size_t)n, &p2));
    CHECK_EQI(p2.latitude_i, -338688000);
    CHECK_EQI(p2.longitude_i, 1512093000);
    CHECK_EQI(p2.altitude, -12);

    SUITE("pb/user-roundtrip");
    meshtastic_User u = meshtastic_User_init_zero;
    strcpy(u.id, "!aabbccdd");
    strcpy(u.long_name, "Lecco Badge");
    strcpy(u.short_name, "LBdg");
    n = mt_user_encode(buf, sizeof buf, &u);
    CHECK(n > 0);
    meshtastic_User u2;
    CHECK(mt_user_decode(buf, (size_t)n, &u2));
    CHECK_STR(u2.id, "!aabbccdd");
    CHECK_STR(u2.long_name, "Lecco Badge");
    CHECK_STR(u2.short_name, "LBdg");

    SUITE("pb/end-to-end-packet");
    /* Data -> encode -> build_packet -> parse_packet -> decode Data. */
    const uint8_t psk[] = {0x01};
    CHECK_EQI(mt_data_make_text(&d, "ping from badge"), 0);
    uint8_t data[256];
    int dlen = mt_data_encode(data, sizeof data, &d);
    CHECK(dlen > 0);
    uint8_t frame[300];
    int flen = mesh_build_packet(frame, sizeof frame, MESH_BROADCAST, 0x12345678u,
                                 0xDEADBEEFu, "LongFast", psk, 1, data, (size_t)dlen, 3, 3, false);
    CHECK(flen > 0);
    CHECK_EQI(frame[13], 0x08);
    mesh_header_t hdr;
    uint8_t plain[256];
    size_t plen = 0;
    CHECK_EQI(mesh_parse_packet(frame, (size_t)flen, "LongFast", psk, 1, &hdr, plain, sizeof plain, &plen), 0);
    CHECK(mt_data_decode(plain, plen, &d2));
    CHECK_EQI(d2.portnum, meshtastic_PortNum_TEXT_MESSAGE_APP);
    CHECK_EQI(d2.payload.size, 15);
    CHECK_MEM(d2.payload.bytes, "ping from badge", 15);
}
