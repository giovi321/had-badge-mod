/* Backend-agnostic message + node types passed across the event bus. */
#ifndef NET_MESSAGE_H
#define NET_MESSAGE_H

#include <stdint.h>
#include <stdbool.h>

#define MSG_TEXT_MAX 234

typedef enum { MSG_TEXT, MSG_POSITION, MSG_NODEINFO } msg_kind_t;

/* Delivery state of an outgoing message (incoming/broadcast stay NONE). */
typedef enum {
    MSG_STATUS_NONE = 0,   /* not tracked (incoming, or broadcast) */
    MSG_STATUS_PENDING,    /* unicast sent, awaiting a delivery ack */
    MSG_STATUS_DELIVERED,  /* delivery ack received */
    MSG_STATUS_FAILED,     /* no ack after retries */
    MSG_STATUS_READ        /* recipient read it (badge-to-badge receipt) */
} msg_status_t;

typedef struct {
    msg_kind_t kind;
    uint32_t from_id;
    uint32_t to_id;            /* recipient (BROADCAST for a broadcast) */
    uint32_t id;              /* packet id (for ack matching) */
    bool outgoing;             /* true if we sent it */
    uint8_t status;            /* msg_status_t: delivery/read state */
    uint8_t channel;           /* index of the channel this message is on */
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
