/* Messages app: Meshtastic chat. WhatsApp-style bubbles (mine right/amber,
 * theirs left/grey), an always-on one-line input (Enter sends), and history.
 *
 * RX events arrive on the radio task; LVGL is single-threaded, so the event
 * handler only copies into a FreeRTOS queue and the app drains it in tick()
 * (which runs in the UI task). */
#include "apps/app_iface.h"
#include "ui/frame.h"
#include "ui/layout.h"
#include "ui/theme.h"
#include "ui/colors.h"
#include "ui/menubar.h"
#include "net/backend.h"
#include "services/services.h"
#include "core/settings.h"
#include "app_config.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "nvs.h"

#define CONTENT_W_70 280   /* ~70% of the 400px content width */

static QueueHandle_t s_pending;
static net_message_t s_hist[MSG_HISTORY_MAX];
static int s_hist_n;
static lv_obj_t *s_list, *s_input, *s_body;
static bool s_active;
static settings_t *s_settings;
static bool s_dirty;            /* history changed since last NVS save */
static int s_save_throttle;     /* bg-tick counter to debounce NVS writes */
static uint32_t s_target = 0xFFFFFFFFu;   /* recipient (broadcast by default) */
static QueueHandle_t s_ack_q;             /* request_ids of delivered messages */
static lv_obj_t *s_to;                    /* "To: ..." label */

void messages_set_target(uint32_t node) { s_target = node; }

/* Compact persisted record (NVS blob), independent of the in-RAM struct. */
typedef struct {
    uint32_t from;
    uint8_t outgoing;
    uint32_t when;
    char name[24];
    char text[MSG_TEXT_MAX + 1];
} msg_rec_t;
#define MSG_BLOB_MAGIC 0x31474d43u  /* "CMG1" */

static const setting_t MSG_SCHEMA[] = {
    {.key = "msg_keep", .type = SET_INT, .def = "50", .label = "Saved messages",
     .group = "Messages", .minv = 0, .maxv = MSG_HISTORY_MAX, .has_min = true, .has_max = true},
};

static void add_bubble(const net_message_t *m);   /* defined below */
static void render_history(void);
static void show_toast(const char *line);

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

static void on_ack(eb_event_t ev, const void *payload, void *ctx)
{
    (void)ev; (void)ctx;
    uint32_t rid = *(const uint32_t *)payload;
    if (s_ack_q) xQueueSend(s_ack_q, &rid, 0);
}

/* --- persistence (NVS "msgs"/"hist") ------------------------------------ */
static void msg_save(void)
{
    int keep = s_settings ? (int)settings_get_int(s_settings, "msg_keep") : 0;
    if (keep < 0) keep = 0;
    if (keep > s_hist_n) keep = s_hist_n;
    int start = s_hist_n - keep;

    size_t blob = 6 + (size_t)keep * sizeof(msg_rec_t);
    uint8_t *buf = malloc(blob);
    if (!buf) return;
    uint8_t *p = buf;
    p[0] = (uint8_t)MSG_BLOB_MAGIC;        p[1] = (uint8_t)(MSG_BLOB_MAGIC >> 8);
    p[2] = (uint8_t)(MSG_BLOB_MAGIC >> 16); p[3] = (uint8_t)(MSG_BLOB_MAGIC >> 24);
    p[4] = (uint8_t)keep; p[5] = (uint8_t)(keep >> 8);
    p += 6;
    for (int i = 0; i < keep; i++) {
        const net_message_t *m = &s_hist[start + i];
        msg_rec_t r;
        memset(&r, 0, sizeof r);
        r.from = m->from_id; r.outgoing = m->outgoing ? 1 : 0; r.when = m->when;
        snprintf(r.name, sizeof r.name, "%s", m->long_name[0] ? m->long_name : m->short_name);
        snprintf(r.text, sizeof r.text, "%s", m->text);
        memcpy(p, &r, sizeof r);
        p += sizeof r;
    }
    nvs_handle_t h;
    if (nvs_open("msgs", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, "hist", buf, blob);
        nvs_commit(h);
        nvs_close(h);
    }
    free(buf);
}

static void msg_load(void)
{
    int keep = s_settings ? (int)settings_get_int(s_settings, "msg_keep") : 0;
    if (keep <= 0) return;
    nvs_handle_t h;
    if (nvs_open("msgs", NVS_READONLY, &h) != ESP_OK) return;
    size_t blob = 0;
    if (nvs_get_blob(h, "hist", NULL, &blob) != ESP_OK || blob < 6) { nvs_close(h); return; }
    uint8_t *buf = malloc(blob);
    if (!buf) { nvs_close(h); return; }
    if (nvs_get_blob(h, "hist", buf, &blob) != ESP_OK) { free(buf); nvs_close(h); return; }
    nvs_close(h);

    uint8_t *p = buf;
    uint32_t magic = p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
    int count = p[4] | (p[5] << 8);
    p += 6;
    if (magic == MSG_BLOB_MAGIC) {
        if (6 + (size_t)count * sizeof(msg_rec_t) > blob) count = (int)((blob - 6) / sizeof(msg_rec_t));
        if (count > MSG_HISTORY_MAX) count = MSG_HISTORY_MAX;
        s_hist_n = 0;
        for (int i = 0; i < count; i++) {
            msg_rec_t r;
            memcpy(&r, p, sizeof r);
            p += sizeof r;
            net_message_t *m = &s_hist[s_hist_n++];
            memset(m, 0, sizeof *m);
            m->kind = MSG_TEXT; m->from_id = r.from; m->outgoing = r.outgoing; m->when = r.when;
            snprintf(m->long_name, sizeof m->long_name, "%s", r.name);
            snprintf(m->text, sizeof m->text, "%s", r.text);
        }
    }
    free(buf);
}

/* Background drain: runs in the UI task whether or not the app is open, so
 * incoming messages are recorded (and persisted) even from other screens. */
static void msg_bg_tick(lv_timer_t *t)
{
    (void)t;
    net_message_t m;
    bool changed = false, got_rx = false;
    char toast[300] = "";
    while (s_pending && xQueueReceive(s_pending, &m, 0) == pdTRUE) {
        hist_push(&m);
        if (s_active) {
            add_bubble(&m);
        } else if (!m.outgoing) {
            got_rx = true;
            const char *who = m.long_name[0] ? m.long_name : (m.short_name[0] ? m.short_name : "node");
            snprintf(toast, sizeof toast, "%s: %s", who, m.text);
        }
        s_dirty = true;
        changed = true;
    }

    /* Delivery acks: mark the matching sent message and repaint to add a check. */
    uint32_t rid;
    bool acked = false;
    while (s_ack_q && xQueueReceive(s_ack_q, &rid, 0) == pdTRUE) {
        for (int i = s_hist_n - 1; i >= 0; i--) {
            if (s_hist[i].outgoing && s_hist[i].id == rid && !s_hist[i].delivered) {
                s_hist[i].delivered = true;
                acked = true;
                break;
            }
        }
    }

    if (changed && s_active) lv_obj_scroll_to_y(s_list, LV_COORD_MAX, LV_ANIM_ON);
    if (acked && s_active) render_history();
    if (got_rx && !s_active) show_toast(toast);
    if (s_dirty && ++s_save_throttle >= 12) {   /* ~ every 5 s */
        msg_save();
        s_dirty = false;
        s_save_throttle = 0;
    }
}

void messages_init(eventbus_t *bus, settings_t *settings)
{
    s_settings = settings;
    if (settings) settings_register_many(settings, MSG_SCHEMA, 1);
    s_pending = xQueueCreate(12, sizeof(net_message_t));
    s_ack_q = xQueueCreate(8, sizeof(uint32_t));
    eventbus_subscribe(bus, EV_MESSAGE_RECEIVED, on_event, NULL);
    eventbus_subscribe(bus, EV_MESSAGE_SENT, on_event, NULL);
    eventbus_subscribe(bus, EV_MESSAGE_ACK, on_ack, NULL);
    msg_load();
    lv_timer_create(msg_bg_tick, 400, NULL);
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
        if (m->delivered) {
            char buf[300];
            snprintf(buf, sizeof buf, "%s  " LV_SYMBOL_OK, m->text);
            lv_label_set_text(bubble, buf);
        } else {
            lv_label_set_text(bubble, m->text);
        }
    } else {
        char buf[300];
        const char *who = m->long_name[0] ? m->long_name : m->short_name;
        snprintf(buf, sizeof buf, "%s\n%s", who[0] ? who : "node", m->text);
        lv_label_set_text(bubble, buf);
    }
}

static lv_obj_t *s_toast;   /* the one live toast; cleared by its DELETE event */

static void toast_deleted(lv_event_t *e) { (void)e; s_toast = NULL; }

static void show_toast(const char *line)
{
    /* One toast at a time: a burst of messages updates it in place instead of
     * stacking overlapping cards on the top layer. */
    if (s_toast) lv_obj_delete(s_toast);
    lv_obj_t *t = lv_obj_create(lv_layer_top());
    s_toast = t;
    lv_obj_add_event_cb(t, toast_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_set_width(t, 300);
    lv_obj_set_height(t, LV_SIZE_CONTENT);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 6, 2);
    lv_obj_add_style(t, &st_card, 0);
    lv_obj_set_style_border_color(t, theme_hex(C_ACCENT), 0);
    lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *l = lv_label_create(t);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_label_set_text(l, line);
    lv_obj_delete_delayed(t, 3500);
}

static void update_to(void)
{
    if (!s_to) return;
    if (s_target == 0xFFFFFFFFu) {                 /* broadcast: muted */
        lv_obj_set_style_text_color(s_to, theme_hex(C_TEXT_DIM), 0);
        lv_label_set_text(s_to, "To: Broadcast (everyone)");
        return;
    }
    char nm[40], b[64];                            /* private: amber, stands out */
    node_record_t *r = nodedb_get(net_nodedb(), s_target);
    if (r) net_node_label(s_target, r->long_name, r->short_name, nm, sizeof nm);
    else net_node_id_str(s_target, nm);
    lv_obj_set_style_text_color(s_to, theme_hex(C_ACCENT), 0);
    snprintf(b, sizeof b, "To: %s (private)", nm);
    lv_label_set_text(s_to, b);
}

/* F2 = "To": cycle the recipient Broadcast -> each heard node -> Broadcast, so
 * choosing who a message goes to never leaves the chat. */
static void cycle_target(void)
{
    nodedb_t *db = net_nodedb();
    if (s_target == 0xFFFFFFFFu) {
        s_target = db->count > 0 ? db->nodes[0].num : 0xFFFFFFFFu;
    } else {
        int idx = -1;
        for (int i = 0; i < db->count; i++)
            if (db->nodes[i].num == s_target) { idx = i; break; }
        s_target = (idx >= 0 && idx + 1 < db->count) ? db->nodes[idx + 1].num : 0xFFFFFFFFu;
    }
    update_to();
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
    net_send_text_to(s_target, txt);   /* echo arrives via EV_MESSAGE_SENT */
    lv_textarea_set_text(s_input, "");
}

static void input_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_READY) send_current();  /* Enter in one-line mode */
}

/* Up/Down scroll the chat history while the input stays focused for typing. */
static void msg_key_cb(lv_event_t *e)
{
    uint32_t k = lv_event_get_key(e);
    /* Clamp at the ends of the history instead of scrolling indefinitely. */
    if (k == LV_KEY_UP) {
        int32_t avail = lv_obj_get_scroll_top(s_list);
        if (avail > 0) lv_obj_scroll_by(s_list, 0, LV_MIN(40, avail), LV_ANIM_OFF);
    } else if (k == LV_KEY_DOWN) {
        int32_t avail = lv_obj_get_scroll_bottom(s_list);
        if (avail > 0) lv_obj_scroll_by(s_list, 0, -LV_MIN(40, avail), LV_ANIM_OFF);
    }
}

static void build(lv_obj_t **screen, lv_group_t *group)
{
    static frame_t f;
    frame_create(&f, "Messages");
    /* No title header in the chat: it only said "Messages". Drop it and let the
     * body use the reclaimed top 20px (the "To:" line is the real context). */
    lv_obj_delete(f.header);
    s_body = f.body;
    lv_obj_set_pos(s_body, CONTENT_X, 0);
    lv_obj_set_height(s_body, CONTENT_BOTTOM);   /* y=0 down to the bar line */
    lv_obj_set_flex_flow(f.body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(f.body, 2, 0);

    s_to = lv_label_create(f.body);
    lv_obj_set_style_text_color(s_to, theme_hex(C_TEXT_DIM), 0);
    lv_obj_set_width(s_to, LV_PCT(100));
    lv_label_set_long_mode(s_to, LV_LABEL_LONG_DOT);

    s_list = lv_obj_create(f.body);
    lv_obj_set_width(s_list, LV_PCT(100));
    lv_obj_set_flex_grow(s_list, 1);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_style_pad_row(s_list, 2, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    /* Clamp scrolling at the first/last message instead of rubber-banding. */
    lv_obj_remove_flag(s_list, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM);

    s_input = lv_textarea_create(f.body);
    lv_textarea_set_one_line(s_input, true);
    lv_textarea_set_placeholder_text(s_input, "Message");
    lv_obj_set_width(s_input, LV_PCT(100));
    lv_obj_add_style(s_input, &st_input, 0);
    lv_obj_add_event_cb(s_input, input_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_input, msg_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(group, s_input);
    lv_group_focus_obj(s_input);

    *screen = f.screen;
    render_history();
    lv_obj_scroll_to_y(s_list, LV_COORD_MAX, LV_ANIM_OFF);
    menubar_set_labels("Send", "To", "Announce", "", "");
    update_to();
    if (s_toast) lv_obj_delete(s_toast);   /* chat is on screen; toast is noise */
    s_active = true;
}

static void on_fkey(int n)
{
    if (n == 1) send_current();
    else if (n == 2) cycle_target();   /* cycle Broadcast <-> nodes */
    else if (n == 3) { mesh_svc_announce_now(); show_toast("Announced to the mesh"); }
}
static void close(void)
{
    s_active = false;
    /* Remember the recipient across app switches: the target now only changes
     * deliberately (Nodes -> Message, or F2 "To"), and the "To:" line always
     * shows it, so there is nothing hidden to reset. */
    if (s_dirty) { msg_save(); s_dirty = false; s_save_throttle = 0; }
}

/* When the bar auto-hides, grow the body into the freed 18px so the chat gets
 * the extra room; shrink it back when the bar returns. */
static void on_bar(bool visible)
{
    if (!s_body) return;
    /* Body runs from y=0 (no header). Bar visible -> stop at the bar line;
     * hidden -> take the full screen height. */
    lv_obj_set_height(s_body, visible ? CONTENT_BOTTOM : SCREEN_H);
    if (s_list) lv_obj_scroll_to_y(s_list, LV_COORD_MAX, LV_ANIM_OFF);
}

const app_def_t *app_messages(void)
{
    static const app_def_t def = {
        .name = "Messages", .icon = LV_SYMBOL_ENVELOPE,
        .build = build, .on_fkey = on_fkey, .close = close,
        .autohide_bar = true, .on_bar = on_bar,
    };
    return &def;
}
