/* UI geometry — the single source of truth for the chrome layout, LVGL-free so
 * the arithmetic is host-tested before it ever reaches a panel.
 *
 * Landscape 428x142. The key structural change from the MicroPython UI:
 *   - the bottom function-key bar spans the FULL width (x=0..428), sitting
 *     UNDER the sidebar, so its text is never clipped at the screen edge;
 *   - the left status sidebar STOPS where it meets that bar
 *     (height = SCREEN_H - BOTTOMBAR_H), instead of running the full height;
 *   - the five F-key cells are evenly distributed (no hand-tuned pixel offsets).
 */
#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#define SCREEN_W      428
#define SCREEN_H      142

#define SIDEBAR_W     28
#define BOTTOMBAR_H   18
#define HEADER_H      20

#define SIDEBAR_H     (SCREEN_H - BOTTOMBAR_H)   /* 124: stops at the bar */
#define CONTENT_X     SIDEBAR_W                  /* 28 */
#define CONTENT_W     (SCREEN_W - SIDEBAR_W)     /* 400 */
#define CONTENT_Y     HEADER_H                   /* 20 */
#define CONTENT_BOTTOM (SCREEN_H - BOTTOMBAR_H)  /* 124 */
#define BODY_H        (CONTENT_BOTTOM - HEADER_H)/* 104 */

#define FKEY_COUNT    5

/* Compute the x-offset and width of evenly distributed F-key cell `i`
 * (0..FKEY_COUNT-1) across the full SCREEN_W. The FKEY_COUNT widths always
 * sum to exactly SCREEN_W and differ by at most 1 px. */
void ui_fkey_cell(int i, int *x, int *w);

#endif /* UI_LAYOUT_H */
