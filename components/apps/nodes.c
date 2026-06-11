/* Nodes app: list of heard Meshtastic nodes (name, SNR, age). */
#include "apps/app_iface.h"
#include "apps/app_manager.h"
#include "ui/frame.h"
#include "ui/theme.h"
#include "ui/colors.h"
#include "ui/menubar.h"
#include "net/backend.h"
#include "net/message.h"
#include "services/services.h"
#include "drivers/gps.h"
#include "util/geo.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *cardinal(double brg)
{
    static const char *c[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    return c[((int)((brg + 22.5) / 45.0)) % 8];
}

static lv_obj_t *s_list;
static lv_group_t *s_group;
static int s_rendered = -1;

static uint32_t focused_num(void)
{
    lv_obj_t *f = lv_group_get_focused(s_group);
    return f ? (uint32_t)(uintptr_t)lv_obj_get_user_data(f) : 0;
}

static void message_focused(void)
{
    uint32_t num = focused_num();
    if (!num) return;
    messages_set_target(num);
    app_manager_launch_app(app_messages());
}

static void track_focused(void)
{
    uint32_t num = focused_num();
    if (!num) return;
    tracker_set_target(num);
    app_manager_launch_app(app_tracker());
}

static void node_key(lv_event_t *e)
{
    uint32_t k = lv_event_get_key(e);
    if (k == LV_KEY_DOWN) lv_group_focus_next(s_group);
    else if (k == LV_KEY_UP) lv_group_focus_prev(s_group);
    /* ENTER activates via node_click (LV_EVENT_CLICKED on the key release), so
     * the launch cannot leak the release into the next screen. */
    lv_obj_t *f = lv_group_get_focused(s_group);
    if (f) lv_obj_scroll_to_view(f, LV_ANIM_ON);
}
static void node_click(lv_event_t *e) { (void)e; message_focused(); }

static void render(void)
{
    nodedb_t *db = net_nodedb();
    uint32_t keep = focused_num();   /* keep focus/scroll across re-renders */
    lv_obj_clean(s_list);
    uint32_t now = (uint32_t)time(NULL);
    gps_fix_t fix;
    bool havegps = gps_get_fix(&fix) && fix.valid;

    if (db->count == 0) {
        lv_obj_t *empty = lv_label_create(s_list);
        lv_label_set_text(empty, "No nodes heard yet");
        lv_obj_set_style_text_color(empty, theme_hex(C_TEXT_DIM), 0);
        s_rendered = 0;
        return;
    }
    for (int i = 0; i < db->count; i++) {
        node_record_t *r = &db->nodes[i];
        char label[64], line[140], nav[28] = "", bat[12] = "";
        net_node_label(r->num, r->long_name, r->short_name, label, sizeof label);
        long age = now ? (long)(now - r->last_heard) : 0;
        if (r->has_telemetry && r->battery) snprintf(bat, sizeof bat, "   %d%%", r->battery);
        if (havegps && r->has_position) {
            double nlat = r->lat_i / 1e7, nlon = r->lon_i / 1e7;
            double dist = geo_distance_m(fix.lat, fix.lon, nlat, nlon);
            const char *card = cardinal(geo_bearing_deg(fix.lat, fix.lon, nlat, nlon));
            if (dist >= 1000.0) snprintf(nav, sizeof nav, "   %.1fkm %s", dist / 1000.0, card);
            else snprintf(nav, sizeof nav, "   %.0fm %s", dist, card);
        }
        snprintf(line, sizeof line, "%s   SNR %.0f   %lds%s%s", label, (double)r->snr, age, bat, nav);

        lv_obj_t *btn = lv_button_create(s_list);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_add_style(btn, &st_card, 0);
        lv_obj_add_style(btn, &st_card_sel, LV_STATE_FOCUSED);
        lv_obj_set_user_data(btn, (void *)(uintptr_t)r->num);
        lv_obj_add_event_cb(btn, node_key, LV_EVENT_KEY, NULL);
        lv_obj_add_event_cb(btn, node_click, LV_EVENT_CLICKED, NULL);
        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, LV_PCT(100));
        lv_label_set_text(l, line);
        if (s_group) lv_group_add_obj(s_group, btn);
    }
    if (keep) {
        uint32_t cnt = lv_obj_get_child_count(s_list);
        for (uint32_t i = 0; i < cnt; i++) {
            lv_obj_t *btn = lv_obj_get_child(s_list, (int32_t)i);
            if ((uint32_t)(uintptr_t)lv_obj_get_user_data(btn) == keep) {
                lv_group_focus_obj(btn);
                lv_obj_scroll_to_view(btn, LV_ANIM_OFF);
                break;
            }
        }
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
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);
    lv_obj_remove_flag(s_list, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM);
    s_rendered = -1;
    render();
    *screen = f.screen;
    menubar_set_labels("Message", "Track", "Announ.", "", "");
}

static void tick(void)
{
    if (net_peer_count() != s_rendered) render();
}

static void on_fkey(int n)
{
    if (n == 1) message_focused();
    else if (n == 2) track_focused();
    else if (n == 3) mesh_svc_announce_now();   /* tell the mesh we're here now */
}

const app_def_t *app_nodes(void)
{
    static const app_def_t def = {
        .name = "Nodes", .icon = LV_SYMBOL_LIST,
        .build = build, .tick = tick, .on_fkey = on_fkey,
    };
    return &def;
}
