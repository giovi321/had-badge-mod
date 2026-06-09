/* See core/settings_nvs.h. */
#include "core/settings_nvs.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "settings_nvs";

typedef struct { settings_store_t base; nvs_handle_t h; bool open; } nvs_store_t;
static nvs_store_t s_store;

static int nvs_get_impl(settings_store_t *self, const char *key, char *buf, int cap)
{
    nvs_store_t *s = (nvs_store_t *)self;
    if (!s->open) return -1;
    size_t len = (size_t)cap;
    if (nvs_get_str(s->h, key, buf, &len) != ESP_OK) return -1;
    return (int)(len > 0 ? len - 1 : 0); /* len includes NUL */
}

static int nvs_set_impl(settings_store_t *self, const char *key, const char *value)
{
    nvs_store_t *s = (nvs_store_t *)self;
    if (!s->open) return -1;
    return nvs_set_str(s->h, key, value) == ESP_OK ? 0 : -1;
}

static void nvs_flush_impl(settings_store_t *self)
{
    nvs_store_t *s = (nvs_store_t *)self;
    if (s->open) nvs_commit(s->h);
}

settings_store_t *settings_nvs_create(const char *ns)
{
    s_store.base.get = nvs_get_impl;
    s_store.base.set = nvs_set_impl;
    s_store.base.flush = nvs_flush_impl;
    if (nvs_open(ns, NVS_READWRITE, &s_store.h) != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open('%s') failed", ns);
        s_store.open = false;
        return &s_store.base; /* still usable; reads fall back to defaults */
    }
    s_store.open = true;
    return &s_store.base;
}
