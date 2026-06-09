/* Breadcrumbs app: start and stop GPS track recording. */
#include "apps/app_iface.h"
#include "ui/frame.h"
#include "ui/theme.h"
#include "ui/colors.h"
#include "ui/menubar.h"
#include "services/track.h"

#include <stdio.h>

static lv_obj_t *s_status, *s_file, *s_hint;

static void refresh_menubar(void)
{
    menubar_set_labels(track_is_active() ? "Stop" : "Start", "", "", "", "Back");
}

static void build(lv_obj_t **screen, lv_group_t *group)
{
    (void)group;
    static frame_t f;
    frame_create(&f, "Breadcrumbs");
    lv_obj_set_flex_flow(f.body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(f.body, 4, 0);

    s_status = lv_label_create(f.body);
    lv_obj_set_style_text_font(s_status, theme_font_title(), 0);
    lv_label_set_text(s_status, "Idle");

    s_file = lv_label_create(f.body);
    lv_label_set_long_mode(s_file, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_file, LV_PCT(100));
    lv_label_set_text(s_file, "");

    s_hint = lv_label_create(f.body);
    lv_obj_set_style_text_color(s_hint, theme_hex(C_TEXT_DIM), 0);
    lv_obj_set_width(s_hint, LV_PCT(100));
    lv_label_set_long_mode(s_hint, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s_hint, "F1 starts and stops recording. A red dot in the sidebar means a track is recording.");

    *screen = f.screen;
    refresh_menubar();
}

static void on_fkey(int n)
{
    if (n != 1) return;
    if (track_is_active()) {
        track_stop();
    } else if (!track_start()) {
        lv_label_set_text(s_hint, "Cannot start: the clock is not set. Enable GPS and wait for a fix first.");
    }
    refresh_menubar();
}

static void tick(void)
{
    char b[64];
    if (track_is_active()) {
        snprintf(b, sizeof b, "Recording  %d pts", track_point_count());
        lv_label_set_text(s_status, b);
        lv_obj_set_style_text_color(s_status, theme_hex(C_CRIT), 0);
        snprintf(b, sizeof b, "File: %s", track_filename());
        lv_label_set_text(s_file, b);
    } else {
        lv_label_set_text(s_status, "Idle");
        lv_obj_set_style_text_color(s_status, theme_hex(C_TEXT), 0);
        if (track_filename()[0]) {
            snprintf(b, sizeof b, "Last: %s", track_filename());
            lv_label_set_text(s_file, b);
        }
    }
}

const app_def_t *app_track(void)
{
    static const app_def_t def = {
        .name = "Breadcrumbs", .icon = LV_SYMBOL_SAVE,
        .build = build, .on_fkey = on_fkey, .tick = tick,
    };
    return &def;
}
