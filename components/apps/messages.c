/* Messages app: Meshtastic chat. WhatsApp-style bubbles (mine right/amber,
 * theirs left/grey), an always-on one-line input (Enter sends), and history.
 *
 * RX events arrive on the radio task; LVGL is single-threaded, so the event
 * handler only copies into a FreeRTOS queue and the app drains it in tick()
 * (which runs in the UI task). */
#include "apps/app_iface.h"
#include "ui/frame.h"
#include "ui/theme.h"
#include "ui/colors.h"
#include "ui/menubar.h"
#include "net/backend.h"
#include "app_config.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define CONTENT_W_70 280   /* ~70% of the 400px content width */

static QueueHandle_t s_pending;
static net_message_t s_hist[MSG_HISTORY_MAX];
static int s_hist_n;
static lv_obj_t *s_list, *s_input;
static bool s_active;

static void hist_push(const net_message_t *m)
{
    if (s_hist_n < MSG_HISTORY_MAX) {
        s_hist[s_hist_n++] = *m;
    } else {
        memmove(&s_hist[0], &s_hist[1], sizeof(net_message_t) * (MSG_HISTORY_MAX - 1));
        s_hist[MSG_HISTORY_MAX - 1] = *m;
    }
}

static void on_event(eb_event_t ev, const void *payload, void *ctx)
{
    (void)ctx;
    const net_message_t *m = payload;
    if (ev != EV_MESSAGE_RECEIVED && ev != EV_MESSAGE_SENT) return;
    if (m->kind != MSG_TEXT) return;
    if (s_pending) xQueueSend(s_pending, m, 0);
}

void messages_init(eventbus_t *bus)
{
    s_pending = xQueueCreate(12, sizeof(net_message_t));
    eventbus_subscribe(bus, EV_MESSAGE_RECEIVED, on_event, NULL);
    eventbus_subscribe(bus, EV_MESSAGE_SENT, on_event, NULL);
}

static void add_bubble(const net_message_t *m)
{
    lv_obj_t *row = lv_obj_create(s_list);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 1, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, m->outgoing ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bubble = lv_label_create(row);
    lv_obj_add_style(bubble, m->outgoing ? &st_bubble_me : &st_bubble_them, 0);
    lv_obj_set_style_max_width(bubble, CONTENT_W_70, 0);
    lv_label_set_long_mode(bubble, LV_LABEL_LONG_WRAP);

    if (m->outgoing) {
        lv_label_set_text(bubble, m->text);
    } else {
        char buf[300];
        const char *who = m->long_name[0] ? m->long_name : m->short_name;
        snprintf(buf, sizeof buf, "%s\n%s", who[0] ? who : "node", m->text);
        lv_label_set_text(bubble, buf);
    }
}

static void render_history(void)
{
    lv_obj_clean(s_list);
    for (int i = 0; i < s_hist_n; i++) add_bubble(&s_hist[i]);
    lv_obj_scroll_to_y(s_list, LV_COORD_MAX, LV_ANIM_OFF);
}

static void send_current(void)
{
    const char *txt = lv_textarea_get_text(s_input);
    if (!txt || !txt[0]) return;
    net_send_text(txt);             /* echo arrives via EV_MESSAGE_SENT */
    lv_textarea_set_text(s_input, "");
}

static void input_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_READY) send_current();  /* Enter in one-line mode */
}

static void build(lv_obj_t **screen, lv_group_t *group)
{
    static frame_t f;
    frame_create(&f, "Messages");
    lv_obj_set_flex_flow(f.body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(f.body, 2, 0);

    s_list = lv_obj_create(f.body);
    lv_obj_set_width(s_list, LV_PCT(100));
    lv_obj_set_flex_grow(s_list, 1);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_style_pad_row(s_list, 2, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);

    s_input = lv_textarea_create(f.body);
    lv_textarea_set_one_line(s_input, true);
    lv_textarea_set_placeholder_text(s_input, "Message");
    lv_obj_set_width(s_input, LV_PCT(100));
    lv_obj_add_style(s_input, &st_input, 0);
    lv_obj_add_event_cb(s_input, input_cb, LV_EVENT_READY, NULL);
    lv_group_add_obj(group, s_input);
    lv_group_focus_obj(s_input);

    *screen = f.screen;
    render_history();
    menubar_set_labels("Send", "", "", "", "");
    s_active = true;
}

static void tick(void)
{
    net_message_t m;
    bool changed = false;
    while (s_pending && xQueueReceive(s_pending, &m, 0) == pdTRUE) { hist_push(&m); if (s_active) add_bubble(&m); changed = true; }
    if (changed && s_active) lv_obj_scroll_to_y(s_list, LV_COORD_MAX, LV_ANIM_ON);
}

static void on_fkey(int n) { if (n == 1) send_current(); }
static void close(void) { s_active = false; }

const app_def_t *app_messages(void)
{
    static const app_def_t def = {
        .name = "Messages", .icon = LV_SYMBOL_ENVELOPE,
        .build = build, .on_fkey = on_fkey, .tick = tick, .close = close,
    };
    return &def;
}
