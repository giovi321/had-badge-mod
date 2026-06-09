/* See ui/frame.h. */
#include "ui/frame.h"
#include "ui/layout.h"
#include "ui/colors.h"
#include "ui/theme.h"

void frame_create(frame_t *f, const char *title)
{
    f->screen = theme_screen_create();

    /* Header: starts at the sidebar's right edge, spans to the screen edge. */
    f->header = lv_obj_create(f->screen);
    lv_obj_set_pos(f->header, CONTENT_X, 0);
    lv_obj_set_size(f->header, CONTENT_W, HEADER_H);
    lv_obj_set_style_bg_color(f->header, theme_hex(C_SURFACE), 0);
    lv_obj_set_style_bg_opa(f->header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(f->header, 0, 0);
    lv_obj_set_style_radius(f->header, 0, 0);
    lv_obj_set_style_pad_hor(f->header, 8, 0);
    lv_obj_set_style_pad_ver(f->header, 0, 0);
    lv_obj_remove_flag(f->header, LV_OBJ_FLAG_SCROLLABLE);

    f->title = lv_label_create(f->header);
    lv_label_set_long_mode(f->title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(f->title, CONTENT_W - 16);
    lv_obj_align(f->title, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_text_color(f->title, theme_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(f->title, theme_font_title(), 0);
    lv_label_set_text(f->title, title ? title : "");

    /* Body: between header and the bottom bar, right of the sidebar. */
    f->body = lv_obj_create(f->screen);
    lv_obj_set_pos(f->body, CONTENT_X, CONTENT_Y);
    lv_obj_set_size(f->body, CONTENT_W, BODY_H);
    lv_obj_set_style_bg_opa(f->body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(f->body, 0, 0);
    lv_obj_set_style_radius(f->body, 0, 0);
    lv_obj_set_style_pad_all(f->body, 4, 0);
}

void frame_set_title(frame_t *f, const char *title)
{
    lv_label_set_text(f->title, title ? title : "");
}

static void scroll_key_cb(lv_event_t *e)
{
    lv_obj_t *c = lv_event_get_target(e);
    uint32_t k = lv_event_get_key(e);
    /* Clamp to the content: scroll only as far as there is content to reveal, so
     * the view stops dead at the top and bottom (lv_obj_scroll_by does not bound
     * itself, which is what made it run past the ends). */
    if (k == LV_KEY_UP) {
        int32_t avail = lv_obj_get_scroll_top(c);
        if (avail > 0) lv_obj_scroll_by(c, 0, LV_MIN(26, avail), LV_ANIM_OFF);
    } else if (k == LV_KEY_DOWN) {
        int32_t avail = lv_obj_get_scroll_bottom(c);
        if (avail > 0) lv_obj_scroll_by(c, 0, -LV_MIN(26, avail), LV_ANIM_OFF);
    }
}

void ui_scroll_focusable(lv_obj_t *cont, lv_group_t *group)
{
    lv_obj_add_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    /* Clamp at the ends: no rubber-band overscroll past the first/last item. */
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_event_cb(cont, scroll_key_cb, LV_EVENT_KEY, NULL);
    if (group) lv_group_add_obj(group, cont);
}
