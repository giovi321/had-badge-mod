/* NVS-backed settings store (device side). Keys must be <= 15 chars (NVS limit). */
#ifndef CORE_SETTINGS_NVS_H
#define CORE_SETTINGS_NVS_H

#include "core/settings.h"

/* Open (or create) an NVS namespace and return a store bound to it. Returns
 * NULL on failure. The returned pointer is statically owned. */
settings_store_t *settings_nvs_create(const char *ns);

#endif /* CORE_SETTINGS_NVS_H */
