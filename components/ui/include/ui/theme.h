/* LVGL theme: one font everywhere (Montserrat 14 body / 18 title), dark/amber
 * color tokens, and the shared styles. Build once at boot via theme_init(). */
#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"
#include "ui/colors.h"

void theme_init(void);

const lv_font_t *theme_font_body(void);
const lv_font_t *theme_font_title(void);

static inline lv_color_t theme_hex(long hex) { return lv_color_hex((uint32_t)hex); }

/* A fresh dark screen object (not loaded). */
lv_obj_t *theme_screen_create(void);

/* Shared styles (built by theme_init). */
extern lv_style_t st_card;
extern lv_style_t st_card_sel;
extern lv_style_t st_input;
extern lv_style_t st_bubble_me;
extern lv_style_t st_bubble_them;
extern lv_style_t st_title;
extern lv_style_t st_hint;

#endif /* UI_THEME_H */
