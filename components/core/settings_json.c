/* See core/settings_json.h. A tiny dependency-free JSON writer/reader for the
 * flat settings map. Only the shapes this firmware emits are handled: a single
 * object of string-or-null values under a "settings" key. */
#include "core/settings_json.h"

#include <string.h>

/* ---- emit --------------------------------------------------------------- */

static bool emit(char *out, int cap, int *n, const char *s)
{
    int len = (int)strlen(s);
    if (*n + len >= cap) return false;     /* keep room for the NUL */
    memcpy(out + *n, s, len);
    *n += len;
    return true;
}

static bool emit_ch(char *out, int cap, int *n, char c)
{
    if (*n + 1 >= cap) return false;
    out[(*n)++] = c;
    return true;
}

static bool emit_escaped(char *out, int cap, int *n, const char *s)
{
    static const char hex[] = "0123456789abcdef";
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') {
            if (!emit_ch(out, cap, n, '\\') || !emit_ch(out, cap, n, (char)c)) return false;
        } else if (c == '\n') { if (!emit(out, cap, n, "\\n")) return false; }
        else if (c == '\r') { if (!emit(out, cap, n, "\\r")) return false; }
        else if (c == '\t') { if (!emit(out, cap, n, "\\t")) return false; }
        else if (c < 0x20) {
            char u[7] = {'\\', 'u', '0', '0', hex[(c >> 4) & 0xf], hex[c & 0xf], 0};
            if (!emit(out, cap, n, u)) return false;
        } else if (!emit_ch(out, cap, n, (char)c)) return false;
    }
    return true;
}

int settings_export_json(const settings_t *reg, bool include_secrets, char *out, int cap)
{
    int n = 0;
    if (cap <= 0) return -1;
    if (!emit(out, cap, &n, "{\"schema\":1,\"settings\":{")) return -1;
    for (int i = 0; i < reg->count; i++) {
        const setting_t *s = reg->items[i];
        if (i && !emit_ch(out, cap, &n, ',')) return -1;
        if (!emit_ch(out, cap, &n, '"')) return -1;
        if (!emit_escaped(out, cap, &n, s->key)) return -1;
        if (!emit(out, cap, &n, "\":")) return -1;
        if (s->secret && !include_secrets) {
            if (!emit(out, cap, &n, "null")) return -1;
        } else {
            char val[200];
            settings_get_str(reg, s->key, val, sizeof val);
            if (!emit_ch(out, cap, &n, '"')) return -1;
            if (!emit_escaped(out, cap, &n, val)) return -1;
            if (!emit_ch(out, cap, &n, '"')) return -1;
        }
    }
    if (!emit(out, cap, &n, "}}")) return -1;
    out[n] = 0;
    return n;
}

/* ---- parse -------------------------------------------------------------- */

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* p points at the opening quote. Decodes the string into buf (truncated to
 * cap-1). Returns the position past the closing quote, or NULL if unterminated. */
static const char *parse_string(const char *p, char *buf, int cap)
{
    if (*p != '"') return NULL;
    p++;
    int j = 0;
    while (*p && *p != '"') {
        char c;
        if (*p == '\\') {
            p++;
            switch (*p) {
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case '/': c = '/'; break;
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                case 'u': {
                    int h1 = hexval(p[1]), h2 = hexval(p[2]), h3 = hexval(p[3]), h4 = hexval(p[4]);
                    if (h1 < 0 || h2 < 0 || h3 < 0 || h4 < 0) return NULL;
                    unsigned cp = (unsigned)((h1 << 12) | (h2 << 8) | (h3 << 4) | h4);
                    p += 4;
                    c = (cp < 0x80) ? (char)cp : '?';   /* settings values are ASCII */
                    break;
                }
                case 0: return NULL;
                default: c = *p; break;
            }
            if (j < cap - 1) buf[j++] = c;
            p++;
        } else {
            if (j < cap - 1) buf[j++] = *p;
            p++;
        }
    }
    if (*p != '"') return NULL;
    buf[j] = 0;
    return p + 1;
}

/* Skip one JSON value we do not accept (number/object/array/literal). */
static const char *skip_value(const char *p)
{
    p = skip_ws(p);
    if (*p == '"') {
        p++;
        while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
        return (*p == '"') ? p + 1 : NULL;
    }
    if (*p == '{' || *p == '[') {
        char open = *p, close = (open == '{') ? '}' : ']';
        int depth = 0;
        while (*p) {
            if (*p == '"') {
                p++;
                while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
                if (*p != '"') return NULL;
                p++;
                continue;
            }
            if (*p == open) depth++;
            else if (*p == close && --depth == 0) return p + 1;
            p++;
        }
        return NULL;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']' &&
           *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
    return p;
}

static bool is_value_end(char c)
{
    return c == ',' || c == '}' || c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0;
}

/* Position just after the '{' of the "settings" object, or NULL. */
static const char *find_settings(const char *json)
{
    const char *p = strstr(json, "\"settings\"");
    if (!p) return NULL;
    p = skip_ws(p + strlen("\"settings\""));
    if (*p != ':') return NULL;
    p = skip_ws(p + 1);
    return (*p == '{') ? p + 1 : NULL;
}

settings_import_result_t settings_import_json(settings_t *reg, const char *json)
{
    settings_import_result_t res = {0, 0, 0, 0};
    const char *p = find_settings(json);
    if (!p) return res;

    for (;;) {
        p = skip_ws(p);
        if (*p == '}' || *p == 0) break;
        if (*p != '"') break;                         /* malformed key */
        char key[64];
        const char *q = parse_string(p, key, sizeof key);
        if (!q) break;
        p = skip_ws(q);
        if (*p != ':') break;
        p = skip_ws(p + 1);

        if (strncmp(p, "null", 4) == 0 && is_value_end(p[4])) {
            res.skipped++;
            p += 4;
        } else if (*p == '"') {
            char val[200];
            q = parse_string(p, val, sizeof val);
            if (!q) break;
            p = q;
            if (!settings_has(reg, key)) res.unknown++;
            else if (settings_set(reg, key, val) == SETTINGS_OK) res.applied++;
            else res.rejected++;
        } else {
            const char *q2 = skip_value(p);
            if (!q2) break;
            p = q2;
            res.rejected++;                           /* non-string value for a setting */
        }

        p = skip_ws(p);
        if (*p == ',') { p++; continue; }
        break;
    }
    return res;
}
