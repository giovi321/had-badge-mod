/* See apps/launcher.h. */
#include "apps/launcher.h"
#include "ui/frame.h"
#include "ui/launcher_strip.h"
#include "ui/menubar.h"
#include "ui/theme.h"
#include "ui/colors.h"
#include "app_config.h"

#include <stdio.h>
#include <string.h>
#include "esp_app_desc.h"

static launcher_select_cb s_cb;
static strip_item_t s_items[8];

static void on_strip(int index, void *ctx) { (void)ctx; if (s_cb) s_cb(index); }

void launcher_build(lv_obj_t **screen, lv_group_t *group,
                    const app_def_t *const *apps, int n, launcher_select_cb cb,
                    int initial_focus)
{
    s_cb = cb;
    if (n > 8) n = 8;
    for (int i = 0; i < n; i++) { s_items[i].name = apps[i]->name; s_items[i].icon = apps[i]->icon; }

    static frame_t f;
    frame_create(&f, "Communicator");

    /* Firmware version, small + muted + left-aligned in the top bar. The commit
     * is the ESP-IDF app version (git short hash, e.g. "0.10.0+1cd08e8"). */
    lv_obj_t *ver = lv_label_create(f.header);
    char vb[40];
    snprintf(vb, sizeof vb, "v%s+%s", FW_VERSION, esp_app_get_description()->version);
    lv_label_set_text(ver, vb);
    lv_obj_align(ver, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_text_color(ver, theme_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(ver, theme_font_body(), 0);

    launcher_strip_create(f.body, s_items, n, on_strip, NULL, group, initial_focus);
    *screen = f.screen;

    menubar_set_labels("Open", "", "", "", "");
}
