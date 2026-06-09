/* See ui/menubar.h. Five flex cells with flex_grow=1 -> evenly distributed,
 * each label centered and clipped (LV_LABEL_LONG_DOT) so text can never run off
 * the screen edge (the old display.f5() clipping bug). */
#include "ui/menubar.h"
#include "ui/layout.h"
#include "ui/colors.h"
#include "ui/theme.h"

#include <ctype.h>
#include <stddef.h>

/* F-key labels are shown in all caps, matching the app headers. */
static void set_caps(lv_obj_t *label, const char *text)
{
    char up[16];
    size_t i = 0;
    if (text) for (; text[i] && i + 1 < sizeof up; i++) up[i] = (char)toupper((unsigned char)text[i]);
    up[i] = 0;
    lv_label_set_text(label, up);
}

static lv_obj_t *s_bar;
static lv_obj_t *s_cell[FKEY_COUNT];
static lv_obj_t *s_label[FKEY_COUNT];
static bool s_visible = true;

void menubar_init(void)
{
    s_bar = lv_obj_create(lv_layer_top());
    lv_obj_set_pos(s_bar, 0, SIDEBAR_H);          /* directly under the sidebar */
    lv_obj_set_size(s_bar, SCREEN_W, BOTTOMBAR_H); /* full width to the left edge */
    lv_obj_set_style_bg_color(s_bar, theme_hex(C_SURFACE), 0);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_bar, 0, 0);
    lv_obj_set_style_radius(s_bar, 0, 0);
    lv_obj_set_style_pad_all(s_bar, 0, 0);
    lv_obj_set_style_pad_column(s_bar, 0, 0);
    lv_obj_remove_flag(s_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < FKEY_COUNT; i++) {
        lv_obj_t *c = lv_obj_create(s_bar);
        lv_obj_set_height(c, LV_PCT(100));
        lv_obj_set_flex_grow(c, 1);                 /* equal widths */
        lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(c, 0, 0);
        lv_obj_set_style_pad_all(c, 0, 0);
        lv_obj_set_style_radius(c, 0, 0);
        /* hairline divider between cells */
        if (i > 0) {
            lv_obj_set_style_border_side(c, LV_BORDER_SIDE_LEFT, 0);
            lv_obj_set_style_border_width(c, 1, 0);
            lv_obj_set_style_border_color(c, theme_hex(C_DIVIDER), 0);
        }
        lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(c, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *l = lv_label_create(c);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, LV_PCT(100));
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(l, theme_hex(C_TEXT), 0);
        lv_obj_set_style_text_font(l, theme_font_body(), 0);
        lv_obj_center(l);
        lv_label_set_text(l, "");
        s_cell[i] = c;
        s_label[i] = l;
    }
}

void menubar_set_labels(const char *l1, const char *l2, const char *l3,
                        const char *l4, const char *l5)
{
    const char *labels[FKEY_COUNT] = { l1, l2, l3, l4, l5 };
    for (int i = 0; i < FKEY_COUNT; i++)
        set_caps(s_label[i], labels[i]);
}

void menubar_set_cell(int i, const char *text)
{
    if (i < 0 || i >= FKEY_COUNT) return;
    set_caps(s_label[i], text);
}

static void slide_cb(void *obj, int32_t y) { lv_obj_set_y((lv_obj_t *)obj, y); }

void menubar_set_visible(bool visible, bool anim)
{
    s_visible = visible;
    int32_t target = visible ? SIDEBAR_H : SCREEN_H;   /* shown at 124, off at 142 */
    lv_anim_delete(s_bar, slide_cb);
    if (!anim) { lv_obj_set_y(s_bar, target); return; }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_bar);
    lv_anim_set_exec_cb(&a, slide_cb);
    lv_anim_set_values(&a, lv_obj_get_y(s_bar), target);
    lv_anim_set_duration(&a, 220);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

bool menubar_visible(void) { return s_visible; }
