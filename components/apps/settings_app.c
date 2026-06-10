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
#include <stdlib.h>

static settings_t *s_reg;
static lv_group_t *s_group;
static frame_t s_frame;
static lv_obj_t *s_body;
static int s_level;            /* 0 groups, 1 list, 2 editor */
static char s_group_name[24];
static const setting_t *s_cur; /* setting being edited at level 2 */
static int s_list_focus;       /* remembered row index at level 1 */
static lv_obj_t *s_edit_ta;    /* text / number entry field */
static lv_obj_t *s_edit_val;   /* number editor value label */
static lv_obj_t *s_slider;     /* number editor slider */
static long s_int_min, s_int_max, s_int_step;  /* number editor bounds */

void settings_app_init(settings_t *reg) { s_reg = reg; }

static void show_groups(void);
static void show_group(const char *group);
static void show_editor(const setting_t *s);

/* --- helpers ------------------------------------------------------------ */
static void clear_body(void)
{
    /* Rebuild triggered from inside a key event (ENTER on a row): swallow the
     * in-flight release so it cannot "click" the next level's focused row
     * (which silently toggled bools and re-opened editors). */
    if (lv_indev_get_active_obj()) lv_indev_wait_release(lv_indev_active());
    lv_group_remove_all_objs(s_group);
    lv_obj_clean(s_body);
    s_edit_ta = NULL;
    s_edit_val = NULL;
    s_slider = NULL;
    lv_obj_set_flex_flow(s_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_body, 3, 0);
    lv_obj_set_scroll_dir(s_body, LV_DIR_VER);
    lv_obj_remove_flag(s_body, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM);
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
/* ENTER activates rows via the LV_EVENT_CLICKED handlers (sent on the key
 * release), never on the press, so the release cannot act on the next level. */
static void group_key(lv_event_t *e) { nav(e); }
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
static void setting_key(lv_event_t *e) { nav(e); }
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
static void enum_key(lv_event_t *e) { nav(e); }
static void enum_click(lv_event_t *e) { pick_choice(lv_event_get_target(e)); }

/* int: slider + numeric entry. The slider mirrors the value; F2/F4 and the Up/Down
 * arrows nudge it by an auto step; typing in the field sets an exact value. */
static void int_refresh(long v)
{
    char b[24];
    snprintf(b, sizeof b, "%ld", v);
    if (s_edit_val) lv_label_set_text(s_edit_val, b);
    if (s_slider) lv_slider_set_value(s_slider, (int32_t)v, LV_ANIM_OFF);
    if (s_edit_ta && strcmp(lv_textarea_get_text(s_edit_ta), b) != 0)
        lv_textarea_set_text(s_edit_ta, b);
}

static void int_commit(long v)
{
    if (v < s_int_min) v = s_int_min;
    if (v > s_int_max) v = s_int_max;
    settings_set_int(s_reg, s_cur->key, v);
    net_reload_config();
    int_refresh(v);
}

static void int_adjust(int dir)   /* dir = -1 / +1, scaled by the auto step */
{
    int_commit(settings_get_int(s_reg, s_cur->key) + (long)dir * s_int_step);
}

/* Live feedback while typing: move the slider/value but commit only on Enter. */
static void int_ta_changed(lv_event_t *e)
{
    (void)e;
    if (!s_edit_ta) return;
    long v = atol(lv_textarea_get_text(s_edit_ta));
    if (v < s_int_min) v = s_int_min;
    if (v > s_int_max) v = s_int_max;
    if (s_slider) lv_slider_set_value(s_slider, (int32_t)v, LV_ANIM_OFF);
    if (s_edit_val) { char b[24]; snprintf(b, sizeof b, "%ld", v); lv_label_set_text(s_edit_val, b); }
}

static void int_ta_key(lv_event_t *e)
{
    uint32_t k = lv_event_get_key(e);
    if (k == LV_KEY_UP) int_adjust(+1);          /* Up/Down nudge; Left/Right = cursor */
    else if (k == LV_KEY_DOWN) int_adjust(-1);
}

static void int_save(void)
{
    if (s_edit_ta) int_commit(atol(lv_textarea_get_text(s_edit_ta)));
    back_to_list();
}
static void int_ta_ready(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_READY) int_save(); }

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
        s_int_min = s->has_min ? s->minv : 0;
        s_int_max = s->has_max ? s->maxv : 1000;
        if (s_int_max <= s_int_min) s_int_max = s_int_min + 1;
        long span = s_int_max - s_int_min;
        s_int_step = span > 50 ? span / 50 : 1;     /* coarse on big ranges */
        long cur = settings_get_int(s_reg, s->key);
        if (cur < s_int_min) cur = s_int_min;
        if (cur > s_int_max) cur = s_int_max;

        s_edit_val = lv_label_create(s_body);       /* big current value */
        lv_obj_set_style_text_font(s_edit_val, theme_font_title(), 0);
        lv_obj_set_style_text_color(s_edit_val, theme_hex(C_ACCENT), 0);

        s_slider = lv_slider_create(s_body);        /* visual position in range */
        lv_obj_set_width(s_slider, LV_PCT(100));
        lv_slider_set_range(s_slider, (int32_t)s_int_min, (int32_t)s_int_max);
        lv_obj_remove_flag(s_slider, LV_OBJ_FLAG_CLICKABLE);   /* set via keys, not touch */
        lv_obj_set_style_bg_color(s_slider, theme_hex(C_SURFACE_2), LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_slider, theme_hex(C_ACCENT), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(s_slider, theme_hex(C_ACCENT), LV_PART_KNOB);

        lv_obj_t *range = lv_label_create(s_body);  /* explain the range */
        lv_obj_add_style(range, &st_hint, 0);
        char rb[48];
        snprintf(rb, sizeof rb, "Range %ld to %ld", s_int_min, s_int_max);
        lv_label_set_text(range, rb);

        s_edit_ta = lv_textarea_create(s_body);     /* type an exact value */
        lv_textarea_set_one_line(s_edit_ta, true);
        lv_textarea_set_accepted_chars(s_edit_ta, "-0123456789");
        lv_obj_set_width(s_edit_ta, LV_PCT(100));
        lv_obj_add_style(s_edit_ta, &st_input, 0);
        lv_obj_add_event_cb(s_edit_ta, int_ta_changed, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(s_edit_ta, int_ta_key, LV_EVENT_KEY, NULL);
        lv_obj_add_event_cb(s_edit_ta, int_ta_ready, LV_EVENT_READY, NULL);
        if (s_group) { lv_group_add_obj(s_group, s_edit_ta); lv_group_focus_obj(s_edit_ta); }

        int_refresh(cur);                           /* fill label + slider + field */

        lv_obj_t *hint = lv_label_create(s_body);
        lv_obj_add_style(hint, &st_hint, 0);
        lv_label_set_text(hint, "Type a value, or F2 / F4 to adjust, then Enter");
        menubar_set_labels("Save", LV_SYMBOL_MINUS, "", LV_SYMBOL_PLUS, "Back");
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
        if (n == 1 && s_cur->type == SET_INT) { int_save(); return; }
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
