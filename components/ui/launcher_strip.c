/* See ui/launcher_strip.h. */
#include "ui/launcher_strip.h"
#include "ui/theme.h"
#include "ui/colors.h"

#include <ctype.h>
#include <stddef.h>

#define TILE_W 104   /* wide enough that names like "Messages" fit on one line */
#define TILE_H 92

/* Tile labels in all caps, matching the headers and the function-key bar. */
static void set_caps(lv_obj_t *label, const char *text)
{
    char up[24];
    size_t i = 0;
    if (text) for (; text[i] && i + 1 < sizeof up; i++) up[i] = (char)toupper((unsigned char)text[i]);
    up[i] = 0;
    lv_label_set_text(label, up);
}

static strip_select_cb s_cb;
static void *s_ctx;
static lv_group_t *s_group;

static void scroll_focused(void)
{
    lv_obj_t *f = lv_group_get_focused(s_group);
    if (f) lv_obj_scroll_to_view(f, LV_ANIM_ON);
}

static void tile_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *tile = lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(tile);

    if (code == LV_EVENT_KEY) {
        uint32_t k = lv_event_get_key(e);
        if (k == LV_KEY_RIGHT || k == LV_KEY_DOWN) { lv_group_focus_next(s_group); scroll_focused(); }
        else if (k == LV_KEY_LEFT || k == LV_KEY_UP) { lv_group_focus_prev(s_group); scroll_focused(); }
        else if (k == LV_KEY_ENTER) { if (s_cb) s_cb(index, s_ctx); }
    } else if (code == LV_EVENT_CLICKED) {
        if (s_cb) s_cb(index, s_ctx);
    } else if (code == LV_EVENT_FOCUSED) {
        scroll_focused();
    }
}

lv_obj_t *launcher_strip_create(lv_obj_t *parent, const strip_item_t *items, int n,
                                strip_select_cb cb, void *ctx, lv_group_t *group,
                                int initial_focus)
{
    s_cb = cb; s_ctx = ctx; s_group = group;
    lv_obj_t *focus_tile = NULL;

    lv_obj_t *strip = lv_obj_create(parent);
    lv_obj_set_size(strip, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(strip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(strip, 0, 0);
    lv_obj_set_style_pad_all(strip, 4, 0);
    lv_obj_set_style_pad_column(strip, 8, 0);
    lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(strip, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(strip, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(strip, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(strip, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < n; i++) {
        lv_obj_t *tile = lv_button_create(strip);
        lv_obj_set_size(tile, TILE_W, TILE_H);
        lv_obj_set_user_data(tile, (void *)(intptr_t)i);
        lv_obj_set_style_bg_color(tile, theme_hex(C_SURFACE), 0);
        lv_obj_set_style_bg_color(tile, theme_hex(C_SURFACE_2), LV_STATE_FOCUSED);
        lv_obj_set_style_border_width(tile, 1, 0);
        lv_obj_set_style_border_color(tile, theme_hex(C_DIVIDER), 0);
        lv_obj_set_style_border_color(tile, theme_hex(C_ACCENT), LV_STATE_FOCUSED);
        lv_obj_set_style_border_width(tile, 2, LV_STATE_FOCUSED);
        lv_obj_set_style_radius(tile, 8, 0);
        lv_obj_set_style_pad_all(tile, 4, 0);
        lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *icon = lv_label_create(tile);
        lv_label_set_text(icon, items[i].icon);
        lv_obj_set_style_text_color(icon, theme_hex(C_ACCENT), 0);
        lv_obj_set_style_text_font(icon, theme_font_title(), 0);

        lv_obj_t *name = lv_label_create(tile);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);   /* single line, ellipsis */
        lv_obj_set_width(name, TILE_W - 8);
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(name, theme_hex(C_TEXT), 0);
        lv_obj_set_style_text_font(name, theme_font_body(), 0);
        set_caps(name, items[i].name);

        lv_obj_add_event_cb(tile, tile_event, LV_EVENT_ALL, NULL);
        if (group) lv_group_add_obj(group, tile);
        if (i == initial_focus) focus_tile = tile;
    }
    if (focus_tile && group) {
        lv_group_focus_obj(focus_tile);
        lv_obj_scroll_to_view(focus_tile, LV_ANIM_OFF);
    }
    return strip;
}
