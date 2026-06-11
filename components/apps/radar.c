/* Radar app ("Find my people"): a PPI scope with you at the centre and every
 * node that has reported a position drawn as a blip at its range and bearing.
 *
 * The badge has no magnetometer yet, so the scope is north-up when you are
 * stopped and heading-up (from GPS course) while you move -- the same rule the
 * Tracker and Follow apps use. scope_up() is the single seam an IMU compass
 * would later replace. The range-and-bearing projection lives in
 * util/radar_proj.c so it can be host-tested without LVGL. */
#include "apps/app_iface.h"
#include "ui/frame.h"
#include "ui/theme.h"
#include "ui/colors.h"
#include "ui/menubar.h"
#include "net/backend.h"
#include "net/message.h"
#include "core/nodedb.h"
#include "drivers/gps.h"
#include "util/geo.h"
#include "util/radar_proj.h"

#include <stdio.h>
#include <math.h>

#define SCOPE   92          /* scope diameter, px (matches the Tracker dial) */
#define CX      46.0f
#define CY      46.0f
#define R_PX    42.0f       /* rim radius for blips (inside the 1px border) */
#define DOT     5           /* node blip size, px */
#define YOU     6           /* centre dot size, px */
#define BLIP_MAX 24         /* node blips drawn at once */

/* Range rings / scope radius the rim represents. -1 = auto (farthest node). */
static const double RANGES[] = {200.0, 1000.0, 5000.0, -1.0};
#define NRANGES ((int)(sizeof RANGES / sizeof RANGES[0]))

static lv_obj_t *s_area, *s_title, *s_info;
static lv_obj_t *s_blip[BLIP_MAX];
static int s_mode;          /* 0 = north-up, 1 = heading-up (when moving) */
static int s_range_idx;     /* index into RANGES */
static uint32_t s_sel;      /* highlighted node, 0 = none */

static lv_obj_t *make_dot(lv_obj_t *parent, int sz, long color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_size(o, sz, sz);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, theme_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static void make_ring(lv_obj_t *parent, int diam)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_size(o, diam, diam);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, theme_hex(C_DIVIDER), 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(o);
}

/* The next positioned node after `after`, cycling through the table. 0 = none. */
static uint32_t next_positioned_num(uint32_t after)
{
    nodedb_t *db = net_nodedb();
    if (db->count == 0) return 0;
    int start = 0;
    for (int i = 0; i < db->count; i++)
        if (db->nodes[i].num == after) { start = i + 1; break; }
    for (int k = 0; k < db->count; k++) {
        node_record_t *r = &db->nodes[(start + k) % db->count];
        if (r->has_position) return r->num;
    }
    return 0;
}

/* Scope up direction in true degrees: heading-up (GPS course) while moving,
 * north-up otherwise. Returns false if there is no usable heading (stopped). */
static bool scope_up(const gps_fix_t *fix, double *up_deg)
{
    bool moving = fix->has_course && fix->speed > 1.0;   /* knots */
    if (s_mode == 1 && moving) { *up_deg = fix->course; return true; }
    *up_deg = 0.0;
    return false;
}

static void set_labels(void)
{
    menubar_set_labels(s_mode ? "North up" : "Head up", "Range", "Next", "", "");
}

static void build(lv_obj_t **screen, lv_group_t *group)
{
    (void)group;
    static frame_t f;
    frame_create(&f, "Radar");
    lv_obj_set_flex_flow(f.body, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(f.body, 6, 0);

    s_area = lv_obj_create(f.body);
    lv_obj_set_size(s_area, SCOPE, SCOPE);
    lv_obj_set_style_radius(s_area, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_area, theme_hex(C_SURFACE), 0);
    lv_obj_set_style_bg_opa(s_area, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_area, 1, 0);
    lv_obj_set_style_border_color(s_area, theme_hex(C_DIVIDER), 0);
    lv_obj_set_style_pad_all(s_area, 0, 0);
    lv_obj_remove_flag(s_area, LV_OBJ_FLAG_SCROLLABLE);

    make_ring(s_area, (int)(R_PX * 2.0f));   /* outer rim */
    make_ring(s_area, (int)R_PX);            /* half-range ring */

    /* "Up" tick at the top of the rim, and you at the centre. */
    lv_obj_t *tick = make_dot(s_area, 2, C_ACCENT);
    lv_obj_set_size(tick, 2, 8);
    lv_obj_set_pos(tick, (int)CX - 1, (int)(CY - R_PX));
    lv_obj_t *you = make_dot(s_area, YOU, C_ACCENT);
    lv_obj_set_pos(you, (int)CX - YOU / 2, (int)CY - YOU / 2);

    for (int i = 0; i < BLIP_MAX; i++) {
        s_blip[i] = make_dot(s_area, DOT, C_TEXT);
        lv_obj_add_flag(s_blip[i], LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t *col = lv_obj_create(f.body);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_PCT(100));
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);

    s_title = lv_label_create(col);
    lv_obj_set_width(s_title, LV_PCT(100));
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(s_title, theme_hex(C_ACCENT), 0);
    s_info = lv_label_create(col);
    lv_obj_set_width(s_info, LV_PCT(100));
    lv_label_set_long_mode(s_info, LV_LABEL_LONG_WRAP);

    *screen = f.screen;
    set_labels();
}

static void on_fkey(int n)
{
    if (n == 1) { s_mode = !s_mode; set_labels(); }
    else if (n == 2) { s_range_idx = (s_range_idx + 1) % NRANGES; }
    else if (n == 3) { s_sel = next_positioned_num(s_sel); }
}

static void fmt_dist(double m, char *out, int cap)
{
    if (m >= 1000.0) snprintf(out, cap, "%.2f km", m / 1000.0);
    else snprintf(out, cap, "%.0f m", m);
}

static void tick(void)
{
    gps_fix_t fix;
    if (!gps_get_fix(&fix) || !fix.valid) {
        lv_label_set_text(s_title, "Radar");
        lv_label_set_text(s_info, "Need your own GPS fix to place nodes.");
        for (int i = 0; i < BLIP_MAX; i++) lv_obj_add_flag(s_blip[i], LV_OBJ_FLAG_HIDDEN);
        return;
    }

    double up;
    bool headingup = scope_up(&fix, &up);
    nodedb_t *db = net_nodedb();

    /* Resolve the range the rim represents (auto = farthest node, 200 m floor). */
    double range = RANGES[s_range_idx];
    if (range < 0.0) {
        double maxd = 200.0;
        for (int i = 0; i < db->count; i++) {
            node_record_t *r = &db->nodes[i];
            if (!r->has_position) continue;
            double dm = geo_distance_m(fix.lat, fix.lon, r->lat_i / 1e7, r->lon_i / 1e7);
            if (dm > maxd) maxd = dm;
        }
        range = maxd;
    }

    int used = 0, shown = 0;
    double nearest = -1.0;
    node_record_t *nr = NULL;
    for (int i = 0; i < db->count && used < BLIP_MAX; i++) {
        node_record_t *r = &db->nodes[i];
        if (!r->has_position) continue;
        radar_blip_t b = radar_project(fix.lat, fix.lon, r->lat_i / 1e7, r->lon_i / 1e7,
                                       up, range, CX, CY, R_PX);
        lv_obj_t *d = s_blip[used++];
        lv_obj_remove_flag(d, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(d, (int)lroundf(b.x) - DOT / 2, (int)lroundf(b.y) - DOT / 2);
        bool hot = (s_sel && r->num == s_sel);
        lv_obj_set_style_bg_color(d, theme_hex(hot ? C_ACCENT : C_TEXT), 0);
        if (b.on_scope) shown++;
        if (nearest < 0.0 || b.dist_m < nearest) { nearest = b.dist_m; nr = r; }
    }
    for (int i = used; i < BLIP_MAX; i++) lv_obj_add_flag(s_blip[i], LV_OBJ_FLAG_HIDDEN);

    char rs[16];
    fmt_dist(range, rs, sizeof rs);
    char t[48];
    snprintf(t, sizeof t, "%s  -  %s",
             s_mode ? (headingup ? "Heading up" : "North up (move)") : "North up", rs);
    lv_label_set_text(s_title, t);

    char info[200];
    if (nr == NULL) {
        snprintf(info, sizeof info,
                 "No node positions yet.\nNodes appear here as they report a position.");
    } else {
        char name[48], ds[24];
        net_node_label(nr->num, nr->long_name, nr->short_name, name, sizeof name);
        fmt_dist(nearest, ds, sizeof ds);
        double brg = geo_bearing_deg(fix.lat, fix.lon, nr->lat_i / 1e7, nr->lon_i / 1e7);
        snprintf(info, sizeof info, "%d on scope of %d\nNearest: %s\n%s, bearing %.0f",
                 shown, used, name, ds, brg);
    }
    lv_label_set_text(s_info, info);
}

const app_def_t *app_radar(void)
{
    static const app_def_t def = {
        .name = "Radar", .icon = LV_SYMBOL_GPS,
        .build = build, .on_fkey = on_fkey, .tick = tick,
    };
    return &def;
}
