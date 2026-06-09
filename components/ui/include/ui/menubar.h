/* Full-width F1-F5 bottom bar on lv_layer_top(), spanning x=0..SCREEN_W under
 * the sidebar. Five evenly distributed cells; text never clips. */
#ifndef UI_MENUBAR_H
#define UI_MENUBAR_H

#include <stdbool.h>

void menubar_init(void);

/* Slide the bar on/off the bottom of the screen. With anim it eases over ~220ms,
 * otherwise it snaps. Used by the app manager's auto-hide policy. */
void menubar_set_visible(bool visible, bool anim);
bool menubar_visible(void);

/* Set the five F-key labels; pass NULL or "" for a blank cell. */
void menubar_set_labels(const char *l1, const char *l2, const char *l3,
                        const char *l4, const char *l5);

/* Set a single F-key cell (0..4). */
void menubar_set_cell(int i, const char *text);

#endif /* UI_MENUBAR_H */
