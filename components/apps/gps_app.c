/* GPS app: shows GPS state, activity, and the current fix. Optional feature.
 *
 * The Status row distinguishes Disabled (GPS off in Settings) from No-data
 * (running but nothing arriving over UART -> wiring/module) from Searching
 * (data flowing, no lock) from Fix. The Data row (sentences + age) is the key
 * field for "is the module even talking?". */
#include "apps/app_iface.h"
#include "ui/frame.h"
#include "ui/theme.h"
#include "ui/colors.h"
#include "ui/menubar.h"
#include "drivers/gps.h"
#include "util/gps_state.h"

#include <stdint.h>
#include <stdio.h>

static lv_obj_t *s_status, *s_sats, *s_qual, *s_lat, *s_lon, *s_alt,
                *s_speed, *s_course, *s_time, *s_data;

static lv_obj_t *row(lv_obj_t *parent, const char *caption)
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
    lv_obj_set_width(cap, 90);
    lv_obj_set_style_text_color(cap, theme_hex(C_TEXT_DIM), 0);
    lv_obj_t *val = lv_label_create(box);
    lv_label_set_text(val, "--");
    return val;
}

static void build(lv_obj_t **screen, lv_group_t *group)
{
    static frame_t f;
    frame_create(&f, "GPS");
    lv_obj_set_flex_flow(f.body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(f.body, 2, 0);
    ui_scroll_focusable(f.body, group);
    s_status = row(f.body, "Status");
    s_sats   = row(f.body, "Satellites");
    s_qual   = row(f.body, "Quality");
    s_lat    = row(f.body, "Latitude");
    s_lon    = row(f.body, "Longitude");
    s_alt    = row(f.body, "Altitude");
    s_speed  = row(f.body, "Speed");
    s_course = row(f.body, "Course");
    s_time   = row(f.body, "Time");
    s_data   = row(f.body, "Data");
    *screen = f.screen;
    menubar_set_labels("", "", "", "", "");
}

static void clear_fix_rows(void)
{
    lv_label_set_text(s_lat, "--");
    lv_label_set_text(s_lon, "--");
    lv_label_set_text(s_alt, "--");
    lv_label_set_text(s_speed, "--");
    lv_label_set_text(s_course, "--");
    lv_label_set_text(s_qual, "--");
    lv_label_set_text(s_time, "--");
}

static void tick(void)
{
    gps_status_t st;
    gps_get_status(&st);
    gps_state_t state = gps_state_from(st.running, st.ms_since_data, st.ms_since_fix, st.fix.valid);
    char b[48];

    switch (state) {
    case GPS_STATE_OFF:       lv_label_set_text(s_status, "Disabled (Settings)"); break;
    case GPS_STATE_NO_DATA:   lv_label_set_text(s_status, "No data - check wiring"); break;
    case GPS_STATE_SEARCHING: lv_label_set_text(s_status, "Searching..."); break;
    case GPS_STATE_FIX:       lv_label_set_text(s_status, "Fix"); break;
    }

    /* Activity: sentences parsed + how long since the last byte. */
    if (state == GPS_STATE_OFF) {
        lv_label_set_text(s_data, "--");
    } else if (st.ms_since_data == UINT32_MAX) {
        snprintf(b, sizeof b, "%lu sent, none yet", (unsigned long)st.sentences);
        lv_label_set_text(s_data, b);
    } else {
        snprintf(b, sizeof b, "%lu sent, %lus ago", (unsigned long)st.sentences,
                 (unsigned long)(st.ms_since_data / 1000));
        lv_label_set_text(s_data, b);
    }

    if (state == GPS_STATE_OFF) {
        lv_label_set_text(s_sats, "--");
        clear_fix_rows();
        return;
    }

    snprintf(b, sizeof b, "%d / %d used/view", st.fix.sats, st.fix.sats_in_view);
    lv_label_set_text(s_sats, b);

    if (state != GPS_STATE_FIX) { clear_fix_rows(); return; }

    snprintf(b, sizeof b, "%.5f", st.fix.lat);  lv_label_set_text(s_lat, b);
    snprintf(b, sizeof b, "%.5f", st.fix.lon);  lv_label_set_text(s_lon, b);
    snprintf(b, sizeof b, "%ld m", (long)st.fix.alt); lv_label_set_text(s_alt, b);
    snprintf(b, sizeof b, "%.1f kn", st.fix.speed);   lv_label_set_text(s_speed, b);
    if (st.fix.has_course) { snprintf(b, sizeof b, "%.0f deg", st.fix.course); lv_label_set_text(s_course, b); }
    else lv_label_set_text(s_course, "--");

    const char *q = st.fix.quality == 2 ? "DGPS" : (st.fix.quality == 1 ? "GPS" : "none");
    snprintf(b, sizeof b, "%s, HDOP %.1f", q, st.fix.hdop);
    lv_label_set_text(s_qual, b);

    if (st.fix.ts) {
        uint32_t secs = st.fix.ts % 86400u;
        snprintf(b, sizeof b, "%02lu:%02lu:%02lu UTC", (unsigned long)(secs / 3600),
                 (unsigned long)((secs % 3600) / 60), (unsigned long)(secs % 60));
        lv_label_set_text(s_time, b);
    } else {
        lv_label_set_text(s_time, "--");
    }
}

const app_def_t *app_gps(void)
{
    static const app_def_t def = {
        .name = "GPS", .icon = LV_SYMBOL_GPS,
        .build = build, .tick = tick,
    };
    return &def;
}
