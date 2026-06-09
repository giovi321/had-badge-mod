/* Schema-driven settings registry (ported from core/settings.py).
 *
 * Each Setting declares a key, type, string default, label, group, and optional
 * validation. Values persist through a pluggable string KV store: an in-memory
 * store for host tests, an NVS-backed store on the device. Everything is stored
 * as strings (ints as "17", bools as "true"/"false"), matching the Python. */
#ifndef CORE_SETTINGS_H
#define CORE_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum { SET_STR, SET_INT, SET_BOOL, SET_ENUM } set_type_t;

typedef enum {
    SETTINGS_OK = 0,
    SETTINGS_ERR_NAN,     /* int field, not a number */
    SETTINGS_ERR_RANGE,   /* int field, out of [minv,maxv] */
    SETTINGS_ERR_ENUM     /* enum field, not in choices */
} settings_err_t;

typedef struct {
    const char *key;
    set_type_t type;
    const char *def;              /* default, as a string */
    const char *label;
    const char *group;
    const char *const *choices;   /* enum choices, or NULL */
    int nchoices;
    long minv, maxv;
    bool has_min, has_max;
    bool secret;
    const char *help;
} setting_t;

/* Pluggable string KV store. Concrete stores put this struct first so they can
 * be cast. get() returns the value length (NUL-terminated in buf) or -1 if the
 * key is absent. */
typedef struct settings_store {
    int  (*get)(struct settings_store *self, const char *key, char *buf, int cap);
    int  (*set)(struct settings_store *self, const char *key, const char *value);
    void (*flush)(struct settings_store *self);
} settings_store_t;

#ifndef SETTINGS_MAX
#define SETTINGS_MAX 48
#endif

typedef struct {
    settings_store_t *store;
    const setting_t *items[SETTINGS_MAX];
    int count;
} settings_t;

void settings_init(settings_t *reg, settings_store_t *store);
bool settings_register(settings_t *reg, const setting_t *s);
void settings_register_many(settings_t *reg, const setting_t *arr, int n);

const setting_t *settings_spec(const settings_t *reg, const char *key);
bool settings_has(const settings_t *reg, const char *key);

/* Typed reads (fall back to the schema default when unset/invalid). */
void settings_get_str(const settings_t *reg, const char *key, char *buf, int cap);
long settings_get_int(const settings_t *reg, const char *key);
bool settings_get_bool(const settings_t *reg, const char *key);

/* Validated writes; persist + flush on success. Unknown keys are stored as-is. */
settings_err_t settings_set(settings_t *reg, const char *key, const char *value);
settings_err_t settings_set_int(settings_t *reg, const char *key, long value);
settings_err_t settings_set_bool(settings_t *reg, const char *key, bool value);

/* Distinct group names in registration order; returns the count. */
int settings_groups(const settings_t *reg, const char *out[], int cap);

/* Display value for UIs: masks secrets ("••••") when set. Writes to buf. */
void settings_display_value(const settings_t *reg, const char *key, char *buf, int cap);

#endif /* CORE_SETTINGS_H */
