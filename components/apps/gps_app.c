/* GPS app: shows the current fix (lat/lon/alt/sats). Optional feature. */
#include "apps/app_iface.h"
#include "ui/frame.h"
#include "ui/theme.h"
#include "ui/colors.h"
#include "ui/menubar.h"
#include "drivers/gps.h"

#include <stdio.h>

static lv_obj_t *s_status, *s_lat, *s_lon, *s_alt, *s_sats;

static lv_obj_t *row(lv_obj_t *parent, const char *caption)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_width(box, LV_PCT(100));
    lv_obj_set_height(box, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 1, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_t *cap = lv_label_create(box);
    lv_label_set_text(cap, caption);
    lv_obj_set_width(cap, 90);
    lv_obj_set_style_text_color(cap, theme_hex(C_TEXT_DIM), 0);
    lv_obj_t *val = lv_label_create(box);
    lv_label_set_text(val, "--");
    return val;
}

static void build(lv_obj_t **screen, lv_group_t *group)
{
    (void)group;
    static frame_t f;
    frame_create(&f, "GPS");
    lv_obj_set_flex_flow(f.body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(f.body, 2, 0);
    s_status = row(f.body, "Status");
    s_lat = row(f.body, "Latitude");
    s_lon = row(f.body, "Longitude");
    s_alt = row(f.body, "Altitude");
    s_sats = row(f.body, "Satellites");
    *screen = f.screen;
    menubar_set_labels("", "", "", "", "");
}

static void tick(void)
{
    gps_fix_t fix;
    bool ok = gps_get_fix(&fix);
    if (!ok || !fix.valid) { lv_label_set_text(s_status, "Searching..."); return; }
    char b[32];
    lv_label_set_text(s_status, "Fix");
    snprintf(b, sizeof b, "%.5f", fix.lat); lv_label_set_text(s_lat, b);
    snprintf(b, sizeof b, "%.5f", fix.lon); lv_label_set_text(s_lon, b);
    snprintf(b, sizeof b, "%ld m", (long)fix.alt); lv_label_set_text(s_alt, b);
    snprintf(b, sizeof b, "%d", fix.sats); lv_label_set_text(s_sats, b);
}

const app_def_t *app_gps(void)
{
    static const app_def_t def = {
        .name = "GPS", .icon = LV_SYMBOL_GPS,
        .build = build, .tick = tick,
    };
    return &def;
}
