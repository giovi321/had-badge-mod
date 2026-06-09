/* See core/eventbus.h. */
#include "core/eventbus.h"
#include <string.h>

void eventbus_init(eventbus_t *bus)
{
    memset(bus, 0, sizeof(*bus));
}

bool eventbus_subscribe(eventbus_t *bus, eb_event_t ev, eb_handler_t fn, void *ctx)
{
    if (ev < 0 || ev >= EV_COUNT) return false;
    int n = bus->count[ev];
    if (n >= EB_MAX_SUBS) return false;
    bus->subs[ev][n].fn = fn;
    bus->subs[ev][n].ctx = ctx;
    bus->count[ev] = n + 1;
    return true;
}

void eventbus_unsubscribe(eventbus_t *bus, eb_event_t ev, eb_handler_t fn)
{
    if (ev < 0 || ev >= EV_COUNT) return;
    int n = bus->count[ev];
    for (int i = 0; i < n; i++) {
        if (bus->subs[ev][i].fn == fn) {
            for (int j = i; j < n - 1; j++) bus->subs[ev][j] = bus->subs[ev][j + 1];
            bus->count[ev] = n - 1;
            return;
        }
    }
}

void eventbus_publish(eventbus_t *bus, eb_event_t ev, const void *payload)
{
    if (ev < 0 || ev >= EV_COUNT) return;
    int n = bus->count[ev];
    for (int i = 0; i < n; i++)
        bus->subs[ev][i].fn(ev, payload, bus->subs[ev][i].ctx);
}

int eventbus_subscriber_count(const eventbus_t *bus, eb_event_t ev)
{
    if (ev < 0 || ev >= EV_COUNT) return 0;
    return bus->count[ev];
}
