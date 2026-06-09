/* Per-app screen chrome: a header (title) + a body inset to avoid the global
 * sidebar and bottom bar. The sidebar/bottom bar live on lv_layer_top() and
 * overlay every screen, so apps only own the header+body. */
#ifndef UI_FRAME_H
#define UI_FRAME_H

#include "lvgl.h"

typedef struct {
    lv_obj_t *screen;   /* lv_screen_load() this to show the app */
    lv_obj_t *header;
    lv_obj_t *title;
    lv_obj_t *body;     /* put app content here */
} frame_t;

void frame_create(frame_t *f, const char *title);
void frame_set_title(frame_t *f, const char *title);

/* Make a container vertically scrollable by Up/Down when focused, and add it to
 * the focus group. For label-only views (no focusable child widgets). */
void ui_scroll_focusable(lv_obj_t *cont, lv_group_t *group);

#endif /* UI_FRAME_H */
