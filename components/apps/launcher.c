/* See apps/launcher.h. */
#include "apps/launcher.h"
#include "ui/frame.h"
#include "ui/launcher_strip.h"
#include "ui/menubar.h"

#include <string.h>

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
    launcher_strip_create(f.body, s_items, n, on_strip, NULL, group, initial_focus);
    *screen = f.screen;

    menubar_set_labels("Open", "", "", "", "");
}
