/* Diagnostics app: live mesh + system stats. */
#include "apps/app_iface.h"
#include "ui/frame.h"
#include "ui/theme.h"
#include "ui/colors.h"
#include "ui/menubar.h"
#include "net/backend.h"
#include "net/message.h"

#include <stdio.h>
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_system.h"

enum { R_NODE, R_RADIO, R_CHAN, R_HOP, R_PEERS, R_RXTX, R_SIG, R_SYS, R_COUNT };
static lv_obj_t *s_val[R_COUNT];

static lv_obj_t *make_row(lv_obj_t *parent, const char *caption)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_width(box, LV_PCT(100));
    lv_obj_set_height(box, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 1, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *cap = lv_label_create(box);
    lv_label_set_text(cap, caption);
    lv_obj_set_width(cap, 64);
    lv_obj_set_style_text_color(cap, theme_hex(C_TEXT_DIM), 0);
    lv_obj_t *val = lv_label_create(box);
    lv_label_set_long_mode(val, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(val, 1);
    lv_obj_set_style_text_color(val, theme_hex(C_TEXT), 0);
    lv_label_set_text(val, "--");
    return val;
}

static void build(lv_obj_t **screen, lv_group_t *group)
{
    static frame_t f;
    frame_create(&f, "Diagnostics");
    lv_obj_set_flex_flow(f.body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(f.body, 1, 0);
    ui_scroll_focusable(f.body, group);   /* Up/Down scrolls the stats */

    s_val[R_NODE]  = make_row(f.body, "Node");
    s_val[R_RADIO] = make_row(f.body, "Radio");
    s_val[R_CHAN]  = make_row(f.body, "Channel");
    s_val[R_HOP]   = make_row(f.body, "Hops");
    s_val[R_PEERS] = make_row(f.body, "Peers");
    s_val[R_RXTX]  = make_row(f.body, "RX/TX");
    s_val[R_SIG]   = make_row(f.body, "Signal");
    s_val[R_SYS]   = make_row(f.body, "System");

    *screen = f.screen;
    menubar_set_labels("", "", "", "", "");
}

static void tick(void)
{
    net_diag_t d;
    net_diag(&d);
    char b[80], id[12];

    net_node_id_str(d.node, id);
    lv_label_set_text(s_val[R_NODE], id);

    snprintf(b, sizeof b, "%s %s %.3fMHz SF%d", d.region, d.preset, d.freq_mhz, d.sf);
    lv_label_set_text(s_val[R_RADIO], b);

    snprintf(b, sizeof b, "%s  sync 0x%02X", d.channel, d.sync_word);
    lv_label_set_text(s_val[R_CHAN], b);

    snprintf(b, sizeof b, "limit %d  relay %s", d.hop_limit, d.relay ? "on" : "off");
    lv_label_set_text(s_val[R_HOP], b);

    snprintf(b, sizeof b, "%d", d.peers);
    lv_label_set_text(s_val[R_PEERS], b);

    snprintf(b, sizeof b, "%lu / %lu pkts", (unsigned long)d.rx_count, (unsigned long)d.tx_count);
    lv_label_set_text(s_val[R_RXTX], b);

    snprintf(b, sizeof b, "RSSI %.0f  SNR %.1f", (double)d.last_rssi, (double)d.last_snr);
    lv_label_set_text(s_val[R_SIG], b);

    uint32_t up = (uint32_t)(esp_timer_get_time() / 1000000);
    snprintf(b, sizeof b, "up %lum%02lus  heap %luK", (unsigned long)(up / 60),
             (unsigned long)(up % 60), (unsigned long)(esp_get_free_heap_size() / 1024));
    lv_label_set_text(s_val[R_SYS], b);
}

const app_def_t *app_diag(void)
{
    static const app_def_t def = {
        .name = "Diag", .icon = LV_SYMBOL_EYE_OPEN,
        .build = build, .tick = tick,
    };
    return &def;
}
