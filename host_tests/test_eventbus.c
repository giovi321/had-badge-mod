/* EventBus pub/sub (mirror of firmware/tests/test_events.py). */
#include "test_util.h"
#include "core/eventbus.h"

static int g_a, g_b;
static void ha(eb_event_t ev, const void *p, void *ctx) { (void)ev; (void)ctx; g_a += *(const int *)p; }
static void hb(eb_event_t ev, const void *p, void *ctx) { (void)ev; (void)p; (void)ctx; g_b++; }

void run_eventbus(void)
{
    SUITE("eventbus");
    eventbus_t bus;
    eventbus_init(&bus);
    g_a = g_b = 0;

    CHECK(eventbus_subscribe(&bus, EV_BATTERY, ha, NULL));
    CHECK(eventbus_subscribe(&bus, EV_BATTERY, hb, NULL));
    CHECK_EQI(eventbus_subscriber_count(&bus, EV_BATTERY), 2);

    int payload = 5;
    eventbus_publish(&bus, EV_BATTERY, &payload);
    CHECK_EQI(g_a, 5);
    CHECK_EQI(g_b, 1);

    /* an event with no subscribers is a no-op */
    eventbus_publish(&bus, EV_KEY, "x");
    CHECK_EQI(eventbus_subscriber_count(&bus, EV_KEY), 0);

    /* unsubscribe one; the other still fires */
    eventbus_unsubscribe(&bus, EV_BATTERY, ha);
    CHECK_EQI(eventbus_subscriber_count(&bus, EV_BATTERY), 1);
    eventbus_publish(&bus, EV_BATTERY, &payload);
    CHECK_EQI(g_a, 5);   /* unchanged */
    CHECK_EQI(g_b, 2);

    /* fill to capacity then reject overflow */
    eventbus_t b2;
    eventbus_init(&b2);
    int ok = 0;
    for (int i = 0; i < EB_MAX_SUBS; i++) ok += eventbus_subscribe(&b2, EV_KEY, hb, NULL) ? 1 : 0;
    CHECK_EQI(ok, EB_MAX_SUBS);
    CHECK(!eventbus_subscribe(&b2, EV_KEY, hb, NULL)); /* full */
}
