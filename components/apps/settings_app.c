/* Settings app: master-detail. Level 0 lists the setting groups, level 1 lists
 * the settings in a group, level 2 is a per-type editor (toggle, choice list,
 * number stepper, or text field). Esc / F5 / Back pops one level. */
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
static lv_group_t *s_group;
static frame_t s_frame;
static lv_obj_t *s_body;
static int s_level;            /* 0 groups, 1 list, 2 editor */
static char s_group_name[24];
static const setting_t *s_cur; /* setting being edited at level 2 */
static int s_list_focus;       /* remembered row index at level 1 */
static lv_obj_t *s_edit_ta;    /* text editor */
static lv_obj_t *s_edit_val;   /* number editor value label */

void settings_app_init(settings_t *reg) { s_reg = reg; }

static void show_groups(void);
static void show_group(const char *group);
static void show_editor(const setting_t *s);

/* --- helpers ------------------------------------------------------------ */
static void clear_body(void)
{
    lv_group_remove_all_objs(s_group);
    lv_obj_clean(s_body);
    s_edit_ta = NULL;
    s_edit_val = NULL;
    lv_obj_set_flex_flow(s_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_body, 3, 0);
    lv_obj_set_scroll_dir(s_body, LV_DIR_VER);
}

static lv_obj_t *make_row(const char *text)
{
    lv_obj_t *btn = lv_button_create(s_body);
    lv_obj_set_width(btn, LV_PCT(100));
    lv_obj_add_style(btn, &st_card, 0);
    lv_obj_add_style(btn, &st_card_sel, LV_STATE_FOCUSED);
    lv_obj_t *l = lv_label_create(btn);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_text(l, text);
    if (s_group) lv_group_add_obj(s_group, btn);
    return btn;
}

static void nav(lv_event_t *e)
{
    uint32_t k = lv_event_get_key(e);
    if (k == LV_KEY_DOWN) lv_group_focus_next(s_group);
    else if (k == LV_KEY_UP) lv_group_focus_prev(s_group);
    lv_obj_t *f = lv_group_get_focused(s_group);
    if (f) lv_obj_scroll_to_view(f, LV_ANIM_ON);
}

static void row_text(const setting_t *s, char *buf, int cap)
{
    char val[80];
    settings_display_value(s_reg, s->key, val, sizeof val);
    snprintf(buf, cap, "%s:  %s", s->label, val);
}

/* --- level 0: groups ---------------------------------------------------- */
static void open_group(lv_obj_t *btn)
{
    const char *name = (const char *)lv_obj_get_user_data(btn);
    if (!name) return;
    snprintf(s_group_name, sizeof s_group_name, "%s", name);
    s_level = 1;
    s_list_focus = 0;
    show_group(s_group_name);
}
static void group_key(lv_event_t *e)
{
    nav(e);
    if (lv_event_get_key(e) == LV_KEY_ENTER) open_group(lv_event_get_target(e));
}
static void group_click(lv_event_t *e) { open_group(lv_event_get_target(e)); }

static void show_groups(void)
{
    clear_body();
    const char *groups[24];
    int ng = settings_groups(s_reg, groups, 24);
    for (int i = 0; i < ng; i++) {
        char buf[40];
        snprintf(buf, sizeof buf, "%s   " LV_SYMBOL_RIGHT, groups[i]);
        lv_obj_t *btn = make_row(buf);
        lv_obj_set_user_data(btn, (void *)groups[i]);
        lv_obj_add_event_cb(btn, group_key, LV_EVENT_KEY, NULL);
        lv_obj_add_event_cb(btn, group_click, LV_EVENT_CLICKED, NULL);
    }
    frame_set_title(&s_frame, "Settings");
    menubar_set_labels("", "", "Open", "", "Back");
}

/* --- level 1: settings in a group --------------------------------------- */
static void edit_setting(lv_obj_t *btn)
{
    int i = (int)(intptr_t)lv_obj_get_user_data(btn);
    const setting_t *s = s_reg->items[i];
    s_list_focus = lv_obj_get_index(btn);
    if (s->type == SET_BOOL) {                       /* toggle in place */
        settings_set_bool(s_reg, s->key, !settings_get_bool(s_reg, s->key));
        net_reload_config();
        char buf[128];
        row_text(s, buf, sizeof buf);
        lv_label_set_text(lv_obj_get_child(btn, 0), buf);
        return;
    }
    s_cur = s;
    s_level = 2;
    show_editor(s);
}
static void setting_key(lv_event_t *e)
{
    nav(e);
    if (lv_event_get_key(e) == LV_KEY_ENTER) edit_setting(lv_event_get_target(e));
}
static void setting_click(lv_event_t *e) { edit_setting(lv_event_get_target(e)); }

static void show_group(const char *name)
{
    clear_body();
    for (int i = 0; i < s_reg->count; i++) {
        const setting_t *s = s_reg->items[i];
        if (strcmp(s->group, name) != 0) continue;
        char buf[128];
        row_text(s, buf, sizeof buf);
        lv_obj_t *btn = make_row(buf);
        lv_obj_set_user_data(btn, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, setting_key, LV_EVENT_KEY, NULL);
        lv_obj_add_event_cb(btn, setting_click, LV_EVENT_CLICKED, NULL);
    }
    frame_set_title(&s_frame, name);
    menubar_set_labels("", "", "Edit", "", "Back");
    lv_obj_t *target = lv_obj_get_child(s_body, s_list_focus);
    if (target) lv_group_focus_obj(target);
}

/* --- level 2: editors --------------------------------------------------- */
static void back_to_list(void) { s_level = 1; show_group(s_group_name); }

/* enum: choice list */
static void pick_choice(lv_obj_t *btn)
{
    const char *choice = (const char *)lv_obj_get_user_data(btn);
    settings_set(s_reg, s_cur->key, choice);
    net_reload_config();
    back_to_list();
}
static void enum_key(lv_event_t *e)
{
    nav(e);
    if (lv_event_get_key(e) == LV_KEY_ENTER) pick_choice(lv_event_get_target(e));
}
static void enum_click(lv_event_t *e) { pick_choice(lv_event_get_target(e)); }

/* int: stepper */
static void int_adjust(int dir)
{
    settings_set_int(s_reg, s_cur->key, settings_get_int(s_reg, s_cur->key) + dir);
    net_reload_config();
    if (s_edit_val) {
        char b[32];
        snprintf(b, sizeof b, "%ld", settings_get_int(s_reg, s_cur->key));
        lv_label_set_text(s_edit_val, b);
    }
}
static void int_key(lv_event_t *e)
{
    uint32_t k = lv_event_get_key(e);
    if (k == LV_KEY_LEFT || k == LV_KEY_DOWN) int_adjust(-1);
    else if (k == LV_KEY_RIGHT || k == LV_KEY_UP) int_adjust(+1);
    else if (k == LV_KEY_ENTER) back_to_list();
}

/* str: text field */
static void str_save(void)
{
    if (!s_edit_ta) return;
    settings_set(s_reg, s_cur->key, lv_textarea_get_text(s_edit_ta));
    net_reload_config();
    back_to_list();
}
static void str_ready(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_READY) str_save(); }

static void show_editor(const setting_t *s)
{
    clear_body();
    frame_set_title(&s_frame, s->label);

    if (s->type == SET_ENUM && s->choices) {
        char cur[40];
        settings_get_str(s_reg, s->key, cur, sizeof cur);
        lv_obj_t *focus = NULL;
        for (int i = 0; i < s->nchoices; i++) {
            lv_obj_t *btn = make_row(s->choices[i]);
            lv_obj_set_user_data(btn, (void *)s->choices[i]);
            lv_obj_add_event_cb(btn, enum_key, LV_EVENT_KEY, NULL);
            lv_obj_add_event_cb(btn, enum_click, LV_EVENT_CLICKED, NULL);
            if (strcmp(s->choices[i], cur) == 0) focus = btn;
        }
        if (focus) lv_group_focus_obj(focus);
        menubar_set_labels("", "", "Select", "", "Back");
    } else if (s->type == SET_INT) {
        lv_obj_t *btn = lv_button_create(s_body);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_add_style(btn, &st_card_sel, 0);
        s_edit_val = lv_label_create(btn);
        lv_obj_set_style_text_font(s_edit_val, theme_font_title(), 0);
        char b[32];
        snprintf(b, sizeof b, "%ld", settings_get_int(s_reg, s->key));
        lv_label_set_text(s_edit_val, b);
        lv_obj_center(s_edit_val);
        lv_obj_add_event_cb(btn, int_key, LV_EVENT_KEY, NULL);
        if (s_group) { lv_group_add_obj(s_group, btn); lv_group_focus_obj(btn); }
        lv_obj_t *hint = lv_label_create(s_body);
        lv_obj_add_style(hint, &st_hint, 0);
        lv_label_set_text(hint, "Left / Right or F2 / F4 to change");
        menubar_set_labels("", LV_SYMBOL_MINUS, "", LV_SYMBOL_PLUS, "Back");
    } else { /* SET_STR */
        char cur[160];
        settings_get_str(s_reg, s->key, cur, sizeof cur);
        s_edit_ta = lv_textarea_create(s_body);
        lv_textarea_set_one_line(s_edit_ta, true);
        lv_textarea_set_text(s_edit_ta, cur);
        lv_obj_set_width(s_edit_ta, LV_PCT(100));
        lv_obj_add_style(s_edit_ta, &st_input, 0);
        lv_obj_add_event_cb(s_edit_ta, str_ready, LV_EVENT_READY, NULL);
        if (s_group) { lv_group_add_obj(s_group, s_edit_ta); lv_group_focus_obj(s_edit_ta); }
        lv_obj_t *hint = lv_label_create(s_body);
        lv_obj_add_style(hint, &st_hint, 0);
        lv_label_set_text(hint, "Type, then Enter or F1 to save");
        menubar_set_labels("Save", "", "", "", "Back");
    }
}

/* --- app interface ------------------------------------------------------ */
static void build(lv_obj_t **screen, lv_group_t *group)
{
    s_group = group;
    frame_create(&s_frame, "Settings");
    s_body = s_frame.body;
    s_level = 0;
    s_group_name[0] = 0;
    s_cur = NULL;
    s_list_focus = 0;
    show_groups();
    *screen = s_frame.screen;
}

static void on_fkey(int n)
{
    lv_obj_t *f = lv_group_get_focused(s_group);
    if (s_level == 2 && s_cur) {
        if (n == 1 && s_cur->type == SET_STR) { str_save(); return; }
        if (n == 2 && s_cur->type == SET_INT) { int_adjust(-1); return; }
        if (n == 4 && s_cur->type == SET_INT) { int_adjust(+1); return; }
        if (n == 3 && s_cur->type == SET_ENUM && f) { pick_choice(f); return; }
    }
    if (n == 3 && f) {
        if (s_level == 0) open_group(f);
        else if (s_level == 1) edit_setting(f);
    }
}

static bool on_back(void)
{
    if (s_level == 2) { back_to_list(); return true; }
    if (s_level == 1) { s_level = 0; show_groups(); return true; }
    return false;   /* at the group list: let the manager return home */
}

const app_def_t *app_settings(void)
{
    static const app_def_t def = {
        .name = "Settings", .icon = LV_SYMBOL_SETTINGS,
        .build = build, .on_fkey = on_fkey, .on_back = on_back,
    };
    return &def;
}
