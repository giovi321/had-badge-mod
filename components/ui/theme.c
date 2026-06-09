/* See ui/theme.h. */
#include "ui/theme.h"

lv_style_t st_card, st_card_sel, st_input, st_bubble_me, st_bubble_them, st_title, st_hint;
static bool s_inited;

const lv_font_t *theme_font_body(void)  { return &lv_font_montserrat_14; }
const lv_font_t *theme_font_title(void) { return &lv_font_montserrat_18; }

void theme_init(void)
{
    if (s_inited) return;
    s_inited = true;

    lv_style_init(&st_card);
    lv_style_set_bg_color(&st_card, theme_hex(C_SURFACE));
    lv_style_set_bg_opa(&st_card, LV_OPA_COVER);
    lv_style_set_radius(&st_card, 8);
    lv_style_set_border_width(&st_card, 1);
    lv_style_set_border_color(&st_card, theme_hex(C_DIVIDER));
    lv_style_set_pad_all(&st_card, 6);
    lv_style_set_text_color(&st_card, theme_hex(C_TEXT));

    lv_style_init(&st_card_sel);
    lv_style_set_bg_color(&st_card_sel, theme_hex(C_SURFACE_2));
    lv_style_set_bg_opa(&st_card_sel, LV_OPA_COVER);
    lv_style_set_radius(&st_card_sel, 8);
    lv_style_set_border_width(&st_card_sel, 2);
    lv_style_set_border_color(&st_card_sel, theme_hex(C_ACCENT));
    lv_style_set_pad_all(&st_card_sel, 6);
    lv_style_set_text_color(&st_card_sel, theme_hex(C_TEXT));

    lv_style_init(&st_input);
    lv_style_set_bg_color(&st_input, theme_hex(C_SURFACE_2));
    lv_style_set_bg_opa(&st_input, LV_OPA_COVER);
    lv_style_set_text_color(&st_input, theme_hex(C_TEXT));
    lv_style_set_radius(&st_input, 9);
    lv_style_set_border_width(&st_input, 1);
    lv_style_set_border_color(&st_input, theme_hex(C_DIVIDER));
    lv_style_set_pad_hor(&st_input, 8);
    lv_style_set_pad_ver(&st_input, 2);

    lv_style_init(&st_bubble_me);
    lv_style_set_bg_color(&st_bubble_me, theme_hex(C_ACCENT));
    lv_style_set_bg_opa(&st_bubble_me, LV_OPA_COVER);
    lv_style_set_text_color(&st_bubble_me, theme_hex(C_ON_ACCENT));
    lv_style_set_radius(&st_bubble_me, 10);
    lv_style_set_pad_hor(&st_bubble_me, 8);
    lv_style_set_pad_ver(&st_bubble_me, 3);

    lv_style_init(&st_bubble_them);
    lv_style_set_bg_color(&st_bubble_them, theme_hex(C_SURFACE_2));
    lv_style_set_bg_opa(&st_bubble_them, LV_OPA_COVER);
    lv_style_set_text_color(&st_bubble_them, theme_hex(C_TEXT));
    lv_style_set_radius(&st_bubble_them, 10);
    lv_style_set_pad_hor(&st_bubble_them, 8);
    lv_style_set_pad_ver(&st_bubble_them, 3);

    lv_style_init(&st_title);
    lv_style_set_text_font(&st_title, theme_font_title());
    lv_style_set_text_color(&st_title, theme_hex(C_TEXT));

    lv_style_init(&st_hint);
    lv_style_set_text_font(&st_hint, theme_font_body());
    lv_style_set_text_color(&st_hint, theme_hex(C_TEXT_DIM));
}

lv_obj_t *theme_screen_create(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, theme_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, theme_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(scr, theme_font_body(), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}
