/* Tracker app: follow a live Meshtastic node, the way you would follow a GPS
 * tracker. Point a needle at the node's last reported position and show what it
 * transmits: distance and bearing from you, the node's own speed and heading,
 * and any environment telemetry (temperature, humidity) it sends.
 *
 * This shares the compass needle with the Follow app and the node positions and
 * telemetry the backend already collects. The difference is the target: a live
 * node from the node DB rather than a saved breadcrumb track. */
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

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define FPI 3.14159265358979323846
#define CX 46
#define CY 46
#define NEEDLE_R 38.0

static uint32_t s_target;
static lv_obj_t *s_area, *s_needle, *s_title, *s_info;
static lv_point_precise_t s_pts[2];

void tracker_set_target(uint32_t node) { s_target = node; }

static void point_north(void)
{
    s_pts[1].x = CX;
    s_pts[1].y = CY - (int)NEEDLE_R;
    lv_line_set_points(s_needle, s_pts, 2);
}

/* The next node after `after` that has a position, cycling through the table. */
static node_record_t *next_positioned(uint32_t after)
{
    nodedb_t *db = net_nodedb();
    if (db->count == 0) return NULL;
    int start = 0;
    for (int i = 0; i < db->count; i++)
        if (db->nodes[i].num == after) { start = i + 1; break; }
    for (int k = 0; k < db->count; k++) {
        node_record_t *r = &db->nodes[(start + k) % db->count];
        if (r->has_position) return r;
    }
    return NULL;
}

static void build(lv_obj_t **screen, lv_group_t *group)
{
    (void)group;
    static frame_t f;
    frame_create(&f, "Tracker");
    lv_obj_set_flex_flow(f.body, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(f.body, 6, 0);

    s_area = lv_obj_create(f.body);
    lv_obj_set_size(s_area, 92, 92);
    lv_obj_set_style_radius(s_area, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_area, theme_hex(C_SURFACE), 0);
    lv_obj_set_style_bg_opa(s_area, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_area, 1, 0);
    lv_obj_set_style_border_color(s_area, theme_hex(C_DIVIDER), 0);
    lv_obj_remove_flag(s_area, LV_OBJ_FLAG_SCROLLABLE);

    s_pts[0].x = CX; s_pts[0].y = CY;
    s_pts[1].x = CX; s_pts[1].y = CY - (int)NEEDLE_R;
    s_needle = lv_line_create(s_area);
    lv_line_set_points(s_needle, s_pts, 2);
    lv_obj_set_style_line_width(s_needle, 4, 0);
    lv_obj_set_style_line_color(s_needle, theme_hex(C_ACCENT), 0);
    lv_obj_set_style_line_rounded(s_needle, true, 0);

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
    menubar_set_labels("Next node", "", "", "", "Back");
}

static void on_fkey(int n)
{
    if (n == 1) {
        node_record_t *r = next_positioned(s_target);
        if (r) s_target = r->num;
    }
}

static void tick(void)
{
    node_record_t *r = s_target ? nodedb_get(net_nodedb(), s_target) : NULL;
    if (!r) { r = next_positioned(0); if (r) s_target = r->num; }

    if (!r) {
        lv_label_set_text(s_title, "No node selected");
        lv_label_set_text(s_info,
            "Open Nodes, choose a tracker and press Track. Or press Next node here.");
        point_north();
        return;
    }

    char name[48];
    net_node_label(r->num, r->long_name, r->short_name, name, sizeof name);
    lv_label_set_text(s_title, name);

    /* What the node itself reports, regardless of our own GPS. */
    char extra[160];
    int p = 0;
    extra[0] = 0;
    if (r->has_motion)
        p += snprintf(extra + p, sizeof extra - p, "Their speed %.1f m/s, course %.0f\n",
                      (double)r->speed, (double)r->course);
    if (r->has_env)
        p += snprintf(extra + p, sizeof extra - p, "Temp %.1f C, hum %.0f%%\n",
                      (double)r->temperature, (double)r->humidity);
    if (r->has_telemetry && r->battery)
        p += snprintf(extra + p, sizeof extra - p, "Battery %d%%\n", r->battery);

    uint32_t now = (uint32_t)time(NULL);
    long age = now ? (long)(now - r->last_heard) : 0;

    if (!r->has_position) {
        char b[220];
        snprintf(b, sizeof b, "%sNo position from this node yet.\nLast heard %lds ago.", extra, age);
        lv_label_set_text(s_info, b);
        point_north();
        return;
    }

    gps_fix_t fix;
    double nlat = r->lat_i / 1e7, nlon = r->lon_i / 1e7;
    if (!gps_get_fix(&fix) || !fix.valid) {
        char b[240];
        snprintf(b, sizeof b, "%sUpdated %lds ago.\nNeed your own GPS fix to show direction.", extra, age);
        lv_label_set_text(s_info, b);
        point_north();
        return;
    }

    double dist = geo_distance_m(fix.lat, fix.lon, nlat, nlon);
    double brg = geo_bearing_deg(fix.lat, fix.lon, nlat, nlon);
    bool moving = fix.has_course && fix.speed > 1.0;   /* knots */
    double rel = moving ? brg - fix.course : brg;
    rel = fmod(rel + 360.0, 360.0);
    double th = rel * FPI / 180.0;
    s_pts[1].x = CX + NEEDLE_R * sin(th);
    s_pts[1].y = CY - NEEDLE_R * cos(th);
    lv_line_set_points(s_needle, s_pts, 2);

    char ds[24];
    if (dist >= 1000.0) snprintf(ds, sizeof ds, "%.2f km", dist / 1000.0);
    else snprintf(ds, sizeof ds, "%.0f m", dist);

    char b[300];
    snprintf(b, sizeof b, "%s away, bearing %.0f\n%s%sUpdated %lds ago",
             ds, brg, extra, moving ? "" : "(north up, move to orient)\n", age);
    lv_label_set_text(s_info, b);
}

const app_def_t *app_tracker(void)
{
    static const app_def_t def = {
        .name = "Tracker", .icon = LV_SYMBOL_GPS,
        .build = build, .on_fkey = on_fkey, .tick = tick,
    };
    return &def;
}
