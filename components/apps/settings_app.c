/* Settings app: scrollable list of all settings. Up/Down navigate; Left/Right
 * (or F2/F4) change the focused value (bool toggles, enum cycles, int +/-1).
 * Schema-driven via the SettingsRegistry. */
#include "apps/app_iface.h"
#include "ui/frame.h"
#include "ui/theme.h"
#include "ui/colors.h"
#include "ui/menubar.h"
#include "core/settings.h"
#include "net/backend.h"

#include <stdio.h>
#include <string.h>

static settings_t *s_reg;
static lv_obj_t *s_list;
static lv_group_t *s_group;
static int s_focus;

void settings_app_init(settings_t *reg) { s_reg = reg; }

static void row_text(const setting_t *s, char *buf, int cap)
{
    char val[80];
    settings_display_value(s_reg, s->key, val, sizeof val);
    snprintf(buf, cap, "%s:  %s", s->label, val);
}

static void adjust(const setting_t *s, int dir)
{
    if (s->type == SET_BOOL) {
        settings_set_bool(s_reg, s->key, !settings_get_bool(s_reg, s->key));
    } else if (s->type == SET_INT) {
        settings_set_int(s_reg, s->key, settings_get_int(s_reg, s->key) + dir);
    } else if (s->type == SET_ENUM && s->choices) {
        char cur[40];
        settings_get_str(s_reg, s->key, cur, sizeof cur);
        int idx = 0;
        for (int i = 0; i < s->nchoices; i++) if (strcmp(s->choices[i], cur) == 0) { idx = i; break; }
        idx = (idx + dir + s->nchoices) % s->nchoices;
        settings_set(s_reg, s->key, s->choices[idx]);
    }
    /* Radio settings changes take effect on next reconfigure/reboot. */
    net_reload_config();
}

static void render(void);

static void row_key(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_KEY) return;
    lv_obj_t *btn = lv_event_get_target(e);
    int i = (int)(intptr_t)lv_obj_get_user_data(btn);
    uint32_t k = lv_event_get_key(e);
    const setting_t *s = s_reg->items[i];
    if (k == LV_KEY_DOWN) { lv_group_focus_next(s_group); }
    else if (k == LV_KEY_UP) { lv_group_focus_prev(s_group); }
    else if (k == LV_KEY_RIGHT || k == LV_KEY_ENTER) { adjust(s, +1); s_focus = i; render(); }
    else if (k == LV_KEY_LEFT) { adjust(s, -1); s_focus = i; render(); }
    lv_obj_t *f = lv_group_get_focused(s_group);
    if (f) lv_obj_scroll_to_view(f, LV_ANIM_ON);
}

static void render(void)
{
    lv_obj_clean(s_list);
    lv_obj_t *focus_target = NULL;
    for (int i = 0; i < s_reg->count; i++) {
        const setting_t *s = s_reg->items[i];
        char buf[128];
        row_text(s, buf, sizeof buf);

        lv_obj_t *btn = lv_button_create(s_list);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_add_style(btn, &st_card, 0);
        lv_obj_add_style(btn, &st_card_sel, LV_STATE_FOCUSED);
        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
        lv_obj_t *l = lv_label_create(btn);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_obj_set_width(l, LV_PCT(100));
        lv_label_set_text(l, buf);
        lv_obj_add_event_cb(btn, row_key, LV_EVENT_KEY, NULL);
        if (s_group) lv_group_add_obj(s_group, btn);
        if (i == s_focus) focus_target = btn;
    }
    if (focus_target) lv_group_focus_obj(focus_target);
}

static void build(lv_obj_t **screen, lv_group_t *group)
{
    static frame_t f;
    frame_create(&f, "Settings");
    s_group = group;
    s_list = lv_obj_create(f.body);
    lv_obj_set_size(s_list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_set_style_pad_row(s_list, 3, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    render();
    *screen = f.screen;
    menubar_set_labels("- Val", "", "Edit", "+ Val", "");
}

static void on_fkey(int n)
{
    if (s_reg->count == 0) return;
    lv_obj_t *f = lv_group_get_focused(s_group);
    int i = f ? (int)(intptr_t)lv_obj_get_user_data(f) : 0;
    if (n == 1 || n == 2) { adjust(s_reg->items[i], -1); s_focus = i; render(); }
    else if (n == 3 || n == 4) { adjust(s_reg->items[i], +1); s_focus = i; render(); }
}

const app_def_t *app_settings(void)
{
    static const app_def_t def = {
        .name = "Settings", .icon = LV_SYMBOL_SETTINGS,
        .build = build, .on_fkey = on_fkey,
    };
    return &def;
}
