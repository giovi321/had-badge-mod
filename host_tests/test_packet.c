/* MeshPacket header + crypto-layer KATs (mirror of firmware/tests/test_packet.py,
 * minus the protobuf decode which is exercised in test_meshtastic_pb.c). */
#include "test_util.h"
#include "mesh/packet.h"

void run_packet(void)
{
    SUITE("packet/header-roundtrip");
    uint8_t h[16];
    mesh_build_header(h, 0xFFFFFFFFu, 0x12345678u, 0xDEADBEEFu,
                      3, 3, false, false, 0x08, 0, 0x78);
    mesh_header_t p;
    mesh_parse_header(h, &p);
    CHECK_EQI(p.to, 0xFFFFFFFFu);
    CHECK_EQI(p.from, 0x12345678u);
    CHECK_EQI(p.id, 0xDEADBEEFu);
    CHECK_EQI(p.hop_limit, 3);
    CHECK_EQI(p.hop_start, 3);
    CHECK_EQI(p.channel, 0x08);
    CHECK_EQI(p.relay_node, 0x78);
    CHECK_MEM(h + 0, "\xFF\xFF\xFF\xFF", 4);
    CHECK_MEM(h + 4, "\x78\x56\x34\x12", 4); /* from, little-endian */

    SUITE("packet/flags");
    CHECK_EQI(mesh_build_flags(3, 3, false, false), 0x63);
    CHECK_EQI(mesh_build_flags(3, 3, true, false), 0x6B);  /* want_ack bit3 */
    CHECK_EQI(mesh_build_flags(7, 7, false, false), 0xE7);

    SUITE("packet/full-roundtrip");
    /* The crypto layer round-trips arbitrary Data bytes; protobuf content is
     * validated separately. Use default LongFast channel -> hash hint 0x08. */
    const uint8_t psk[] = {0x01};
    uint8_t data[] = {0x08, 0x01, 0x12, 0x0f, 'p','i','n','g',' ','f','r','o','m',' ','b','a','d','g','e'};
    uint8_t frame[64];
    int flen = mesh_build_packet(frame, sizeof frame, MESH_BROADCAST, 0x12345678u,
                                 0xDEADBEEFu, "LongFast", psk, 1, data, sizeof data, 3, 3, false);
    CHECK_EQI(flen, (int)(16 + sizeof data));
    CHECK_EQI(frame[13], 0x08); /* channel hint */

    mesh_header_t hdr;
    uint8_t plain[64];
    size_t plen = 0;
    int rc = mesh_parse_packet(frame, (size_t)flen, "LongFast", psk, 1, &hdr, plain, sizeof plain, &plen);
    CHECK_EQI(rc, 0);
    CHECK_EQI(hdr.from, 0x12345678u);
    CHECK_EQI(hdr.id, 0xDEADBEEFu);
    CHECK_EQI(plen, sizeof data);
    CHECK_MEM(plain, data, sizeof data);

    SUITE("packet/reject");
    /* wrong channel -> -2 */
    CHECK_EQI(mesh_parse_packet(frame, (size_t)flen, "AnotherChan", psk, 1, &hdr, plain, sizeof plain, &plen), -2);
    /* short / NULL -> -1 */
    uint8_t shortf[8] = {0};
    CHECK_EQI(mesh_parse_packet(shortf, 8, "LongFast", psk, 1, &hdr, plain, sizeof plain, &plen), -1);
    CHECK_EQI(mesh_parse_packet(NULL, 0, "LongFast", psk, 1, &hdr, plain, sizeof plain, &plen), -1);

    SUITE("packet/decrement-hop");
    int newhop = mesh_decrement_hop(frame, (size_t)flen, 0x42);
    CHECK_EQI(newhop, 2);
    CHECK_EQI(frame[15], 0x42);
    mesh_parse_header(frame, &hdr);
    CHECK_EQI(hdr.hop_limit, 2);
    CHECK_EQI(hdr.hop_start, 3); /* unchanged */
}
