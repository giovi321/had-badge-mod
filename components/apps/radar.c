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
#include "util/map_proj.h"
#include "util/vmap.h"
#include "esp_heap_caps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* Map overlay tuning. */
#define MAP_MARGIN  1.15        /* enlarge the cull box so rim-crossers survive */
#define MAP_MIN_MS  500         /* min interval between overlay redraws (ms) */
#define MAP_UP_EPS  3.0         /* heading change that forces a redraw (deg) */

static lv_obj_t *s_area, *s_title, *s_info;
static lv_obj_t *s_blip[BLIP_MAX];
static int s_mode;          /* 0 = north-up, 1 = heading-up (when moving) */
static int s_range_idx;     /* index into RANGES */
static uint32_t s_sel;      /* highlighted node, 0 = none */

static lv_obj_t *s_map;         /* RGB565 canvas behind the blips */
static void *s_map_buf;         /* canvas backing store (heap) */
static settings_t *s_reg;       /* registry for the radar_map toggle */
static bool s_map_on;           /* overlay enabled */
static bool s_map_present;      /* a readable /spiffs/map.vmap exists */
static bool s_map_valid;        /* the canvas matches s_drawn_* below */
static double s_drawn_lat, s_drawn_lon, s_drawn_up, s_drawn_range;
static uint32_t s_drawn_ms;     /* lv_tick at the last redraw */

static const setting_t RADAR_SCHEMA[] = {
    {.key = "radar_map", .type = SET_BOOL, .def = "false",
     .label = "Map overlay", .group = "Radar"},
};

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
    menubar_set_labels(s_mode ? "North up" : "Head up", "Range", "Next",
                       s_map_on ? "Map*" : "Map", "");
}

static double ang_diff(double a, double b)
{
    double d = fmod(a - b + 540.0, 360.0) - 180.0;
    return d < 0 ? -d : d;
}

/* Redraw the map canvas for the given view: clear to the scope colour, then (if
 * the overlay is on and a map file is present) stream the .vmap, cull
 * out-of-view features by bbox, project each kept polyline with the same
 * orientation as the blips and clip it to the rim. Called only when the view
 * changed -- never every tick. */
static void map_redraw(double clat, double clon, double up, double range)
{
    if (!s_map || !s_map_buf) return;
    lv_canvas_fill_bg(s_map, theme_hex(C_SURFACE), LV_OPA_COVER);
    if (!s_map_on || range <= 0.0) return;

    vmap_reader_t r;
    if (vmap_open(&r, VMAP_DEFAULT_PATH) != 0) { s_map_present = false; return; }
    s_map_present = true;

    map_bbox_e7_t view;
    map_view_bbox(clat, clon, range, MAP_MARGIN, &view);

    lv_layer_t layer;
    lv_canvas_init_layer(s_map, &layer);

    lv_draw_line_dsc_t road, water;
    lv_draw_line_dsc_init(&road);
    road.color = theme_hex(C_TEXT_DIM); road.width = 1; road.opa = LV_OPA_COVER;
    lv_draw_line_dsc_init(&water);
    water.color = theme_hex(C_CHARGE); water.width = 1; water.opa = LV_OPA_COVER;

    static int32_t pts[2 * VMAP_MAX_POINTS];   /* static: off the LVGL task stack */
    vmap_feature_t ft;
    while (vmap_next(&r, &ft) == 1) {
        if (!map_bbox_overlap(&view, &ft.bbox)) { vmap_skip_points(&r, &ft); continue; }
        int n = vmap_read_points(&r, &ft, pts, (int)(sizeof pts / sizeof pts[0]));
        if (n < 2) continue;
        lv_draw_line_dsc_t *dsc = (ft.cls == VMAP_CLASS_WATER) ? &water : &road;
        float px, py;
        map_project(clat, clon, pts[0] / VMAP_E7, pts[1] / VMAP_E7,
                    up, range, CX, CY, R_PX, &px, &py);
        for (int i = 1; i < n; i++) {
            float qx, qy, vx0, vy0, vx1, vy1;
            map_project(clat, clon, pts[2 * i] / VMAP_E7, pts[2 * i + 1] / VMAP_E7,
                        up, range, CX, CY, R_PX, &qx, &qy);
            if (map_clip_segment_px(px, py, qx, qy, CX, CY, R_PX,
                                    &vx0, &vy0, &vx1, &vy1)) {
                dsc->p1.x = (int32_t)lroundf(vx0); dsc->p1.y = (int32_t)lroundf(vy0);
                dsc->p2.x = (int32_t)lroundf(vx1); dsc->p2.y = (int32_t)lroundf(vy1);
                lv_draw_line(&layer, dsc);
            }
            px = qx; py = qy;
        }
    }
    vmap_close(&r);
    lv_canvas_finish_layer(s_map, &layer);
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
    lv_obj_set_style_clip_corner(s_area, true, 0);   /* mask the canvas to the rim */

    /* Map overlay canvas, created before the rings/blips so it sits behind them. */
    s_map = lv_canvas_create(s_area);
    lv_obj_set_pos(s_map, 0, 0);
    lv_obj_set_size(s_map, SCOPE, SCOPE);
    lv_obj_set_style_pad_all(s_map, 0, 0);
    lv_obj_set_style_border_width(s_map, 0, 0);
    lv_obj_remove_flag(s_map, LV_OBJ_FLAG_SCROLLABLE);
    size_t mbsz = LV_CANVAS_BUF_SIZE(SCOPE, SCOPE, 16, LV_DRAW_BUF_STRIDE_ALIGN);
    s_map_buf = heap_caps_malloc(mbsz, MALLOC_CAP_SPIRAM);
    if (!s_map_buf) s_map_buf = heap_caps_malloc(mbsz, MALLOC_CAP_DEFAULT);
    if (s_map_buf) {
        lv_canvas_set_buffer(s_map, s_map_buf, SCOPE, SCOPE, LV_COLOR_FORMAT_RGB565);
        lv_canvas_fill_bg(s_map, theme_hex(C_SURFACE), LV_OPA_COVER);
    }
    s_map_valid = false;
    s_map_present = false;

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
    else if (n == 4) {
        s_map_on = !s_map_on;
        if (s_reg) settings_set_bool(s_reg, "radar_map", s_map_on);
        s_map_valid = false;   /* force a redraw (or a clear) next tick */
        if (!s_map_on && s_map && s_map_buf)
            lv_canvas_fill_bg(s_map, theme_hex(C_SURFACE), LV_OPA_COVER);
        set_labels();
    }
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
        if (s_map && s_map_buf) lv_canvas_fill_bg(s_map, theme_hex(C_SURFACE), LV_OPA_COVER);
        s_map_valid = false;   /* can't place the map without a fix */
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

    /* Map overlay: recompute only when the view actually moved, rotated or
     * zoomed -- the canvas is far too costly to redraw at the 10 Hz tick. */
    if (s_map_on) {
        bool need = !s_map_valid
                 || fabs(range - s_drawn_range) > s_drawn_range * 0.02
                 || geo_distance_m(s_drawn_lat, s_drawn_lon, fix.lat, fix.lon) > range / 40.0
                 || ang_diff(up, s_drawn_up) > MAP_UP_EPS;
        if (need && (!s_map_valid || lv_tick_elaps(s_drawn_ms) >= MAP_MIN_MS)) {
            map_redraw(fix.lat, fix.lon, up, range);
            s_drawn_lat = fix.lat; s_drawn_lon = fix.lon;
            s_drawn_up = up; s_drawn_range = range;
            s_map_valid = true;
            s_drawn_ms = lv_tick_get();
        }
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
    if (s_map_on && !s_map_present) {
        size_t l = strlen(info);
        snprintf(info + l, sizeof info - l, "\nNo map uploaded (see /map).");
    }
    lv_label_set_text(s_info, info);
}

static void close_app(void)
{
    if (s_map_buf) { free(s_map_buf); s_map_buf = NULL; }
    s_map = NULL;
    s_map_valid = false;
    s_map_present = false;
}

/* Register the radar settings and load the saved overlay state (call once at
 * boot, before the settings app builds its form). */
void radar_init(settings_t *reg)
{
    s_reg = reg;
    if (reg) {
        settings_register_many(reg, RADAR_SCHEMA,
                               (int)(sizeof RADAR_SCHEMA / sizeof RADAR_SCHEMA[0]));
        s_map_on = settings_get_bool(reg, "radar_map");
    }
}

const app_def_t *app_radar(void)
{
    static const app_def_t def = {
        .name = "Radar", .icon = LV_SYMBOL_GPS,
        .build = build, .on_fkey = on_fkey, .tick = tick, .close = close_app,
    };
    return &def;
}
