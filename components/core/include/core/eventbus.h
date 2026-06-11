/* Tiny synchronous publish/subscribe event bus (ported from core/events.py).
 *
 * Handlers run on the publisher's stack, in subscription order. Publishers
 * include the radio RX path, so handlers MUST be short, non-blocking, and MUST
 * NOT call LVGL — for UI work, set a flag/snapshot and let the UI task redraw.
 * Fixed-capacity, no malloc. */
#ifndef CORE_EVENTBUS_H
#define CORE_EVENTBUS_H

#include <stdbool.h>

typedef enum {
    EV_POSITION_UPDATE,   /* const position_t*  */
    EV_MESSAGE_RECEIVED,  /* const net_message_t* */
    EV_MESSAGE_SENT,      /* const net_message_t* */
    EV_MESSAGE_ACK,       /* const uint32_t* request_id (delivery ack) */
    EV_MESSAGE_FAILED,    /* const uint32_t* packet id (no ack after retries) */
    EV_MESH_NODE_UPDATE,  /* const node_record_t* */
    EV_WIFI_STATE,        /* const wifi_state_t* */
    EV_BATTERY,           /* const battery_state_msg_t* */
    EV_BATTERY_LOW,       /* const battery_state_msg_t* */
    EV_TIME_SYNC,         /* const uint32_t* epoch seconds */
    EV_BACKEND_CHANGED,   /* const char* backend name */
    EV_MESSAGES_READ,     /* NULL */
    EV_KEY,               /* const char* one key (opt-in) */
    EV_COUNT
} eb_event_t;

typedef void (*eb_handler_t)(eb_event_t ev, const void *payload, void *ctx);

#ifndef EB_MAX_SUBS
#define EB_MAX_SUBS 8
#endif

typedef struct {
    struct { eb_handler_t fn; void *ctx; } subs[EV_COUNT][EB_MAX_SUBS];
    int count[EV_COUNT];
} eventbus_t;

void eventbus_init(eventbus_t *bus);
/* Returns false if the per-event subscriber table is full. */
bool eventbus_subscribe(eventbus_t *bus, eb_event_t ev, eb_handler_t fn, void *ctx);
void eventbus_unsubscribe(eventbus_t *bus, eb_event_t ev, eb_handler_t fn);
void eventbus_publish(eventbus_t *bus, eb_event_t ev, const void *payload);
int  eventbus_subscriber_count(const eventbus_t *bus, eb_event_t ev);

#endif /* CORE_EVENTBUS_H */
