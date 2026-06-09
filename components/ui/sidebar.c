/* See ui/sidebar.h. */
#include "ui/sidebar.h"
#include "ui/layout.h"
#include "ui/colors.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/theme_map.h"

static lv_obj_t *s_bar;
static lv_obj_t *s_batt, *s_mesh, *s_wifi, *s_gps, *s_track;

static lv_obj_t *make_icon(lv_obj_t *parent, const char *sym)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, sym);
    lv_obj_set_style_text_color(l, theme_hex(C_IDLE), 0);
    lv_obj_set_style_text_font(l, theme_font_body(), 0);
    return l;
}

void sidebar_init(void)
{
    s_bar = lv_obj_create(lv_layer_top());
    lv_obj_set_pos(s_bar, 0, 0);
    lv_obj_set_size(s_bar, SIDEBAR_W, SIDEBAR_H);
    lv_obj_set_style_bg_color(s_bar, theme_hex(C_SIDEBAR), 0);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_bar, 0, 0);
    lv_obj_set_style_radius(s_bar, 0, 0);
    lv_obj_set_style_pad_all(s_bar, 2, 0);
    lv_obj_remove_flag(s_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(s_bar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_batt = make_icon(s_bar, LV_SYMBOL_BATTERY_EMPTY);
    s_mesh = make_icon(s_bar, ICON_MESH);
    s_wifi = make_icon(s_bar, ICON_WIFI);
    s_gps  = make_icon(s_bar, ICON_GPS);

    /* breadcrumb recording dot (drawn, not a glyph, so no font dependency) */
    s_track = lv_obj_create(s_bar);
    lv_obj_set_size(s_track, 8, 8);
    lv_obj_set_style_radius(s_track, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_track, theme_hex(C_IDLE), 0);
    lv_obj_set_style_bg_opa(s_track, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_track, 0, 0);
    lv_obj_remove_flag(s_track, LV_OBJ_FLAG_SCROLLABLE);
}

void sidebar_update(const status_snapshot_t *s)
{
    if (!s_bar) return;

    /* Battery */
    lv_label_set_text(s_batt, icon_battery(s->batt_present ? s->batt_pct : -1, s->charging));
    long bc = theme_battery_fill_color(s->batt_present ? s->batt_pct : -1, s->charging, s->batt_present);
    lv_obj_set_style_text_color(s_batt, theme_hex(bc == C_NONE ? C_IDLE : bc), 0);

    /* Mesh */
    int ml = theme_mesh_level(s->mesh_up, s->mesh_peers);
    lv_obj_set_style_text_color(s_mesh, theme_hex(ml > 0 ? C_ACCENT : C_IDLE), 0);

    /* WiFi */
    int wl = theme_wifi_level(s->wifi_state, s->wifi_rssi, s->wifi_rssi_valid);
    lv_obj_set_style_text_color(s_wifi, theme_hex(wl > 0 ? theme_wifi_color(s->wifi_state) : C_IDLE), 0);

    /* GPS */
    gps_state_t gs = theme_gps_state(s->gps_enabled, s->gps_fix, s->gps_sats);
    long gc = (gs == GPS_OFF) ? C_IDLE : (gs == GPS_SEARCH ? C_TEXT_DIM : C_OK);
    lv_obj_set_style_text_color(s_gps, theme_hex(gc), 0);

    /* Tracking: red dot when recording, dim otherwise. */
    lv_obj_set_style_bg_color(s_track, theme_hex(s->tracking ? C_CRIT : C_IDLE), 0);
}
