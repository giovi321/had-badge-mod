/* See core/settings.h. */
#include "core/settings.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TRUE_WORDS[] = {"1", "true", "yes", "on"};

static void copy_str(char *dst, int cap, const char *src)
{
    if (cap <= 0) return;
    int i = 0;
    for (; src && src[i] && i < cap - 1; i++) dst[i] = src[i];
    dst[i] = 0;
}

static bool parse_bool(const char *s)
{
    char low[16];
    int i = 0;
    while (s && s[i] == ' ') i++;
    int j = 0;
    for (; s && s[i] && j < (int)sizeof(low) - 1; i++, j++) {
        char c = s[i];
        low[j] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    low[j] = 0;
    for (size_t k = 0; k < sizeof(TRUE_WORDS) / sizeof(TRUE_WORDS[0]); k++)
        if (strcmp(low, TRUE_WORDS[k]) == 0) return true;
    return false;
}

/* Parse a base-10 integer tolerating surrounding spaces; *ok=false on junk. */
static long parse_int(const char *s, bool *ok)
{
    while (*s == ' ') s++;
    char *end;
    long v = strtol(s, &end, 10);
    while (*end == ' ') end++;
    *ok = (end != s && *end == 0);
    return v;
}

static bool in_choices(const setting_t *s, const char *value)
{
    if (!s->choices) return true;
    for (int i = 0; i < s->nchoices; i++)
        if (strcmp(s->choices[i], value) == 0) return true;
    return false;
}

void settings_init(settings_t *reg, settings_store_t *store)
{
    reg->store = store;
    reg->count = 0;
}

bool settings_register(settings_t *reg, const setting_t *s)
{
    for (int i = 0; i < reg->count; i++)
        if (strcmp(reg->items[i]->key, s->key) == 0) { reg->items[i] = s; return true; }
    if (reg->count >= SETTINGS_MAX) return false;
    reg->items[reg->count++] = s;
    return true;
}

void settings_register_many(settings_t *reg, const setting_t *arr, int n)
{
    for (int i = 0; i < n; i++) settings_register(reg, &arr[i]);
}

const setting_t *settings_spec(const settings_t *reg, const char *key)
{
    for (int i = 0; i < reg->count; i++)
        if (strcmp(reg->items[i]->key, key) == 0) return reg->items[i];
    return NULL;
}

bool settings_has(const settings_t *reg, const char *key)
{
    return settings_spec(reg, key) != NULL;
}

void settings_get_str(const settings_t *reg, const char *key, char *buf, int cap)
{
    const setting_t *s = settings_spec(reg, key);
    char raw[160];
    int n = reg->store ? reg->store->get(reg->store, key, raw, sizeof raw) : -1;
    if (!s) { copy_str(buf, cap, n >= 0 ? raw : ""); return; }
    if (n < 0) { copy_str(buf, cap, s->def); return; }
    if (s->type == SET_ENUM && !in_choices(s, raw)) { copy_str(buf, cap, s->def); return; }
    copy_str(buf, cap, raw);
}

long settings_get_int(const settings_t *reg, const char *key)
{
    const setting_t *s = settings_spec(reg, key);
    char raw[160];
    int n = reg->store ? reg->store->get(reg->store, key, raw, sizeof raw) : -1;
    if (n < 0) { bool ok; long d = parse_int(s ? s->def : "0", &ok); return ok ? d : 0; }
    bool ok;
    long v = parse_int(raw, &ok);
    if (ok) return v;
    bool dok; long d = parse_int(s ? s->def : "0", &dok);
    return dok ? d : 0;
}

bool settings_get_bool(const settings_t *reg, const char *key)
{
    const setting_t *s = settings_spec(reg, key);
    char raw[160];
    int n = reg->store ? reg->store->get(reg->store, key, raw, sizeof raw) : -1;
    return parse_bool(n < 0 ? (s ? s->def : "false") : raw);
}

settings_err_t settings_set(settings_t *reg, const char *key, const char *value)
{
    const setting_t *s = settings_spec(reg, key);
    if (!s) {
        if (reg->store) { reg->store->set(reg->store, key, value); reg->store->flush(reg->store); }
        return SETTINGS_OK;
    }
    char enc[160];
    if (s->type == SET_INT) {
        bool ok;
        long v = parse_int(value, &ok);
        if (!ok) return SETTINGS_ERR_NAN;
        if (s->has_min && v < s->minv) return SETTINGS_ERR_RANGE;
        if (s->has_max && v > s->maxv) return SETTINGS_ERR_RANGE;
        snprintf(enc, sizeof enc, "%ld", v);
    } else if (s->type == SET_BOOL) {
        copy_str(enc, sizeof enc, parse_bool(value) ? "true" : "false");
    } else if (s->type == SET_ENUM) {
        if (!in_choices(s, value)) return SETTINGS_ERR_ENUM;
        copy_str(enc, sizeof enc, value);
    } else {
        copy_str(enc, sizeof enc, value);
    }
    if (reg->store) { reg->store->set(reg->store, key, enc); reg->store->flush(reg->store); }
    return SETTINGS_OK;
}

settings_err_t settings_set_int(settings_t *reg, const char *key, long value)
{
    char buf[32];
    snprintf(buf, sizeof buf, "%ld", value);
    return settings_set(reg, key, buf);
}

settings_err_t settings_set_bool(settings_t *reg, const char *key, bool value)
{
    return settings_set(reg, key, value ? "true" : "false");
}

int settings_groups(const settings_t *reg, const char *out[], int cap)
{
    int n = 0;
    for (int i = 0; i < reg->count; i++) {
        const char *g = reg->items[i]->group;
        bool seen = false;
        for (int j = 0; j < n; j++) if (strcmp(out[j], g) == 0) { seen = true; break; }
        if (!seen && n < cap) out[n++] = g;
    }
    return n;
}

void settings_display_value(const settings_t *reg, const char *key, char *buf, int cap)
{
    const setting_t *s = settings_spec(reg, key);
    char val[160];
    settings_get_str(reg, key, val, sizeof val);
    if (s && s->secret && val[0]) { copy_str(buf, cap, "\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2"); return; }
    copy_str(buf, cap, val);
}
