/* Thin encode/decode helpers over the nanopb-generated Meshtastic messages.
 *
 * The generated structs (meshtastic_Data/Position/User) are fixed-size and
 * heap-free; these wrappers just drive pb_encode/pb_decode and provide a couple
 * of convenience builders. Field numbers/types come straight from the official
 * protobufs (see components/mesh/proto/meshtastic.proto). */
#ifndef MESH_MESHTASTIC_PB_H
#define MESH_MESHTASTIC_PB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "meshtastic.pb.h"  /* nanopb generated */

/* Encoders return the encoded byte count, or -1 if it would overflow cap. */
int mt_data_encode(uint8_t *buf, size_t cap, const meshtastic_Data *d);
int mt_position_encode(uint8_t *buf, size_t cap, const meshtastic_Position *p);
int mt_user_encode(uint8_t *buf, size_t cap, const meshtastic_User *u);

/* Decoders zero-init the struct first and return false on malformed input. */
bool mt_data_decode(const uint8_t *buf, size_t len, meshtastic_Data *d);
bool mt_position_decode(const uint8_t *buf, size_t len, meshtastic_Position *p);
bool mt_user_decode(const uint8_t *buf, size_t len, meshtastic_User *u);

/* Build a TEXT_MESSAGE_APP Data wrapping a UTF-8 string. Returns 0, or -1 if
 * the text is longer than the payload field can hold. */
int mt_data_make_text(meshtastic_Data *d, const char *text);

#endif /* MESH_MESHTASTIC_PB_H */
