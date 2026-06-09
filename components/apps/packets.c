/* Packet log app: a rolling list of recently received mesh packets. */
#include "apps/app_iface.h"
#include "ui/frame.h"
#include "ui/theme.h"
#include "ui/colors.h"
#include "ui/menubar.h"
#include "net/backend.h"
#include "net/message.h"

#include <stdio.h>
#include <time.h>

static lv_obj_t *s_list;
static int s_last_count = -1;
static uint32_t s_last_when, s_last_from;

static const char *portname(uint8_t p)
{
    switch (p) {
    case 1:  return "text";
    case 3:  return "pos";
    case 4:  return "info";
    case 5:  return "route";
    case 67: return "telem";
    case 70: return "trace";
    default: return "app";
    }
}

static void render(void)
{
    net_pkt_log_t e[32];
    int n = net_packet_log(e, 32);
    lv_obj_clean(s_list);
    if (n == 0) {
        lv_obj_t *l = lv_label_create(s_list);
        lv_label_set_text(l, "No packets yet");
        lv_obj_set_style_text_color(l, theme_hex(C_TEXT_DIM), 0);
        s_last_count = 0;
        return;
    }
    for (int i = 0; i < n; i++) {
        char id[12], line[80], ts[10] = "--:--:--";
        net_node_id_str(e[i].from, id);
        if (e[i].when) {
            time_t t = e[i].when;
            struct tm tmv;
            gmtime_r(&t, &tmv);
            strftime(ts, sizeof ts, "%H:%M:%S", &tmv);
        }
        snprintf(line, sizeof line, "%s  %s  %s  %ddB", ts, id, portname(e[i].portnum), e[i].rssi);
        lv_obj_t *row = lv_label_create(s_list);
        lv_label_set_long_mode(row, LV_LABEL_LONG_DOT);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_style_text_font(row, theme_font_body(), 0);
        lv_label_set_text(row, line);
    }
    s_last_count = n;
    if (n) { s_last_when = e[0].when; s_last_from = e[0].from; }
}

static void build(lv_obj_t **screen, lv_group_t *group)
{
    static frame_t f;
    frame_create(&f, "Packets");
    s_list = lv_obj_create(f.body);
    lv_obj_set_size(s_list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_style_pad_row(s_list, 1, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    ui_scroll_focusable(s_list, group);
    s_last_count = -1;
    render();
    *screen = f.screen;
    menubar_set_labels("", "", "", "", "");
}

static void tick(void)
{
    net_pkt_log_t e[1];
    int n = net_packet_log(e, 1);
    if (n) {
        if (e[0].when != s_last_when || e[0].from != s_last_from) render();
    } else if (s_last_count != 0) {
        render();
    }
}

const app_def_t *app_packets(void)
{
    static const app_def_t def = {
        .name = "Packets", .icon = LV_SYMBOL_BARS,
        .build = build, .tick = tick,
    };
    return &def;
}
