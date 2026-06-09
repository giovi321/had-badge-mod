/* Backend-agnostic message + node types passed across the event bus. */
#ifndef NET_MESSAGE_H
#define NET_MESSAGE_H

#include <stdint.h>
#include <stdbool.h>

#define MSG_TEXT_MAX 234

typedef enum { MSG_TEXT, MSG_POSITION, MSG_NODEINFO } msg_kind_t;

typedef struct {
    msg_kind_t kind;
    uint32_t from_id;
    bool outgoing;             /* true if we sent it */
    char text[MSG_TEXT_MAX + 1];
    double lat, lon;
    int32_t alt;
    int sats;
    uint32_t ts;               /* unix seconds for position */
    char long_name[48];
    char short_name[16];
    float snr;
    int rssi;
    uint32_t when;             /* local receipt time */
} net_message_t;

/* "!aabbccdd" node id string into buf (>= 10 bytes). */
void net_node_id_str(uint32_t node, char *buf);

/* Best display label for a node number, using a known short/long name if any. */
void net_node_label(uint32_t node, const char *long_name, const char *short_name,
                    char *buf, int cap);

#endif /* NET_MESSAGE_H */
