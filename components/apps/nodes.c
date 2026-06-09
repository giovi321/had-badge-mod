/* Nodes app: list of heard Meshtastic nodes (name, SNR, age). */
#include "apps/app_iface.h"
#include "ui/frame.h"
#include "ui/theme.h"
#include "ui/colors.h"
#include "ui/menubar.h"
#include "net/backend.h"
#include "net/message.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static lv_obj_t *s_list;
static lv_group_t *s_group;
static int s_rendered = -1;

static void render(void)
{
    nodedb_t *db = net_nodedb();
    lv_obj_clean(s_list);
    uint32_t now = (uint32_t)time(NULL);

    if (db->count == 0) {
        lv_obj_t *empty = lv_label_create(s_list);
        lv_label_set_text(empty, "No nodes heard yet");
        lv_obj_set_style_text_color(empty, theme_hex(C_TEXT_DIM), 0);
        s_rendered = 0;
        return;
    }
    for (int i = 0; i < db->count; i++) {
        node_record_t *r = &db->nodes[i];
        char label[64], line[96];
        net_node_label(r->num, r->long_name, r->short_name, label, sizeof label);
        long age = now ? (long)(now - r->last_heard) : 0;
        snprintf(line, sizeof line, "%s   SNR %.0f   %lds", label, (double)r->snr, age);

        lv_obj_t *btn = lv_button_create(s_list);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_add_style(btn, &st_card, 0);
        lv_obj_add_style(btn, &st_card_sel, LV_STATE_FOCUSED);
        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, LV_PCT(100));
        lv_label_set_text(l, line);
        if (s_group) lv_group_add_obj(s_group, btn);
    }
    s_rendered = db->count;
}

static void build(lv_obj_t **screen, lv_group_t *group)
{
    static frame_t f;
    frame_create(&f, "Nodes");
    s_group = group;
    s_list = lv_obj_create(f.body);
    lv_obj_set_size(s_list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_style_pad_row(s_list, 3, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    s_rendered = -1;
    render();
    *screen = f.screen;
    menubar_set_labels("", "", "", "", "");
}

static void tick(void)
{
    if (net_peer_count() != s_rendered) render();
}

const app_def_t *app_nodes(void)
{
    static const app_def_t def = {
        .name = "Nodes", .icon = LV_SYMBOL_LIST,
        .build = build, .tick = tick,
    };
    return &def;
}
