/* Follow app: load the most recent track and point a needle back along it.
 *
 * The badge has no magnetometer, so heading comes from GPS course over ground.
 * The needle is relative to your direction of travel while you move, and falls
 * back to north-up when you are stopped. */
#include "apps/app_iface.h"
#include "ui/frame.h"
#include "ui/theme.h"
#include "ui/colors.h"
#include "ui/menubar.h"
#include "services/track.h"
#include "drivers/gps.h"
#include "util/geo.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <math.h>

#define CAP 1024
#define ARRIVE_M 12.0
#define FPI 3.14159265358979323846
#define CX 46
#define CY 46
#define NEEDLE_R 38.0

static double s_lat[CAP], s_lon[CAP];
static int s_n;
static int s_target;
static char s_loaded[40];
static lv_obj_t *s_area, *s_needle, *s_info, *s_file;
static lv_point_precise_t s_pts[2];

static void load_latest(void)
{
    s_n = 0;
    s_loaded[0] = 0;
    DIR *d = opendir(TRACK_DIR);
    if (!d) return;
    char best[40] = {0};
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "trk_", 4) != 0) continue;
        if (strcmp(e->d_name, best) > 0) snprintf(best, sizeof best, "%.39s", e->d_name);
    }
    closedir(d);
    if (!best[0]) return;

    char path[80];
    snprintf(path, sizeof path, "%s/%s", TRACK_DIR, best);
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[96];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        unsigned long ts;
        double la, lo;
        if (sscanf(line, "%lu,%lf,%lf", &ts, &la, &lo) == 3 && s_n < CAP) {
            s_lat[s_n] = la;
            s_lon[s_n] = lo;
            s_n++;
        }
    }
    fclose(f);
    snprintf(s_loaded, sizeof s_loaded, "%.39s", best);
    s_target = s_n - 1;   /* start near where the track ends, walk back to 0 */
}

static void build(lv_obj_t **screen, lv_group_t *group)
{
    (void)group;
    static frame_t f;
    frame_create(&f, "Follow back");
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

    s_info = lv_label_create(col);
    lv_obj_set_width(s_info, LV_PCT(100));
    lv_label_set_long_mode(s_info, LV_LABEL_LONG_WRAP);
    s_file = lv_label_create(col);
    lv_obj_set_style_text_color(s_file, theme_hex(C_TEXT_DIM), 0);
    lv_obj_set_width(s_file, LV_PCT(100));
    lv_label_set_long_mode(s_file, LV_LABEL_LONG_DOT);

    load_latest();
    *screen = f.screen;
    menubar_set_labels("Reload", "", "", "", "Back");
}

static void on_fkey(int n) { if (n == 1) load_latest(); }

static void tick(void)
{
    if (s_n < 2) {
        lv_label_set_text(s_info, "No saved track found. Record one in Breadcrumbs.");
        lv_label_set_text(s_file, "");
        return;
    }
    gps_fix_t fix;
    if (!gps_get_fix(&fix) || !fix.valid) {
        lv_label_set_text(s_info, "Waiting for a GPS fix...");
        return;
    }

    double dt = geo_distance_m(fix.lat, fix.lon, s_lat[s_target], s_lon[s_target]);
    while (dt < ARRIVE_M && s_target > 0) {
        s_target--;
        dt = geo_distance_m(fix.lat, fix.lon, s_lat[s_target], s_lon[s_target]);
    }

    double brg = geo_bearing_deg(fix.lat, fix.lon, s_lat[s_target], s_lon[s_target]);
    bool moving = fix.has_course && fix.speed > 1.0;   /* knots */
    double rel = moving ? brg - fix.course : brg;
    rel = fmod(rel + 360.0, 360.0);
    double th = rel * FPI / 180.0;
    s_pts[1].x = CX + NEEDLE_R * sin(th);
    s_pts[1].y = CY - NEEDLE_R * cos(th);
    lv_line_set_points(s_needle, s_pts, 2);

    char b[180];
    if (s_target == 0 && dt < ARRIVE_M) {
        snprintf(b, sizeof b, "Back at the start.");
    } else {
        snprintf(b, sizeof b, "To waypoint: %.0f m\nBearing %.0f deg\nWaypoint %d of %d\n%s",
                 dt, brg, s_target + 1, s_n, moving ? "relative to travel" : "north up, move to orient");
    }
    lv_label_set_text(s_info, b);
    lv_label_set_text(s_file, s_loaded);
}

const app_def_t *app_follow(void)
{
    static const app_def_t def = {
        .name = "Follow", .icon = LV_SYMBOL_UP,
        .build = build, .on_fkey = on_fkey, .tick = tick,
    };
    return &def;
}
