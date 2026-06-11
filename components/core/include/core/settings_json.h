/* JSON backup/restore for the settings registry. Dependency-free (no LVGL/IDF,
 * no cJSON) so it host-tests: the format is one flat object,
 *   {"schema":1,"settings":{"<key>":"<value>", ...}}
 * with secret settings emitted as their real value or as null. */
#ifndef CORE_SETTINGS_JSON_H
#define CORE_SETTINGS_JSON_H

#include "core/settings.h"

/* Serialise every registered setting. Secret settings are written with their
 * real value when include_secrets is true, or as null otherwise (so a backup
 * can leave out channel keys / passwords). Returns the byte length written
 * (NUL-terminated in out), or -1 if it would not fit in cap. */
int settings_export_json(const settings_t *reg, bool include_secrets, char *out, int cap);

typedef struct {
    int applied;   /* known keys validated and persisted */
    int skipped;   /* members present as null (e.g. omitted secrets) */
    int rejected;  /* known keys that failed validation, or non-string values */
    int unknown;   /* members not in the registry (ignored) */
} settings_import_result_t;

/* Apply a JSON object produced by settings_export_json: each string member is
 * run through settings_set (validated + persisted), null members are skipped,
 * and unknown keys are ignored. Tolerates malformed input (no crash, counts
 * stay zero). Locates the inner object via its "settings" key. */
settings_import_result_t settings_import_json(settings_t *reg, const char *json);

#endif /* CORE_SETTINGS_JSON_H */
