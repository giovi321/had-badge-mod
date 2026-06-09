/* SettingsRegistry KATs (mirror of firmware/tests/test_settings.py) using an
 * in-memory KV store. */
#include "test_util.h"
#include "core/settings.h"
#include <string.h>

/* --- in-memory store ---------------------------------------------------- */
typedef struct {
    settings_store_t base;
    char keys[32][32];
    char vals[32][160];
    int n;
    int flushed;
} mem_store_t;

static int mem_get(settings_store_t *self, const char *key, char *buf, int cap)
{
    mem_store_t *m = (mem_store_t *)self;
    for (int i = 0; i < m->n; i++)
        if (strcmp(m->keys[i], key) == 0) {
            int j = 0;
            for (; m->vals[i][j] && j < cap - 1; j++) buf[j] = m->vals[i][j];
            buf[j] = 0;
            return j;
        }
    return -1;
}
static int mem_set(settings_store_t *self, const char *key, const char *value)
{
    mem_store_t *m = (mem_store_t *)self;
    for (int i = 0; i < m->n; i++)
        if (strcmp(m->keys[i], key) == 0) { strncpy(m->vals[i], value, 159); m->vals[i][159] = 0; return 0; }
    if (m->n >= 32) return -1;
    strncpy(m->keys[m->n], key, 31);
    strncpy(m->vals[m->n], value, 159);
    m->n++;
    return 0;
}
static void mem_flush(settings_store_t *self) { ((mem_store_t *)self)->flushed++; }

static void mem_init(mem_store_t *m)
{
    memset(m, 0, sizeof(*m));
    m->base.get = mem_get;
    m->base.set = mem_set;
    m->base.flush = mem_flush;
}
static const char *mem_raw(mem_store_t *m, const char *key)
{
    for (int i = 0; i < m->n; i++) if (strcmp(m->keys[i], key) == 0) return m->vals[i];
    return "";
}

/* --- schema ------------------------------------------------------------- */
static const char *REGION_CHOICES[] = {"EU_868", "US"};
static const setting_t SCHEMA[] = {
    {.key = "device_name", .type = SET_STR,  .def = "Badge-0000", .label = "Device name", .group = "Device"},
    {.key = "tx_power",    .type = SET_INT,  .def = "9",          .label = "TX power",     .group = "Radio",
     .minv = 0, .maxv = 22, .has_min = true, .has_max = true},
    {.key = "wifi_on",     .type = SET_BOOL, .def = "false",      .label = "WiFi",         .group = "WiFi"},
    {.key = "region",      .type = SET_ENUM, .def = "EU_868",     .label = "Region",       .group = "Radio",
     .choices = REGION_CHOICES, .nchoices = 2},
    {.key = "wifi_pw",     .type = SET_STR,  .def = "",           .label = "WiFi password", .group = "WiFi", .secret = true},
};

void run_settings(void)
{
    mem_store_t store;
    mem_init(&store);
    settings_t reg;
    settings_init(&reg, &store.base);
    settings_register_many(&reg, SCHEMA, (int)(sizeof SCHEMA / sizeof SCHEMA[0]));

    char buf[64];

    SUITE("settings/defaults");
    settings_get_str(&reg, "device_name", buf, sizeof buf);
    CHECK_STR(buf, "Badge-0000");
    CHECK_EQI(settings_get_int(&reg, "tx_power"), 9);
    CHECK(!settings_get_bool(&reg, "wifi_on"));
    settings_get_str(&reg, "region", buf, sizeof buf);
    CHECK_STR(buf, "EU_868");

    SUITE("settings/set-get-persist");
    CHECK_EQI(settings_set_int(&reg, "tx_power", 17), SETTINGS_OK);
    CHECK_EQI(settings_get_int(&reg, "tx_power"), 17);
    CHECK_STR(mem_raw(&store, "tx_power"), "17"); /* stored as string */
    CHECK(store.flushed >= 1);
    CHECK_EQI(settings_set_bool(&reg, "wifi_on", true), SETTINGS_OK);
    CHECK(settings_get_bool(&reg, "wifi_on"));
    CHECK_STR(mem_raw(&store, "wifi_on"), "true");

    SUITE("settings/int-validation");
    CHECK_EQI(settings_set(&reg, "tx_power", "99"), SETTINGS_ERR_RANGE);
    CHECK_EQI(settings_set(&reg, "tx_power", "notnum"), SETTINGS_ERR_NAN);

    SUITE("settings/enum-validation");
    CHECK_EQI(settings_set(&reg, "region", "US"), SETTINGS_OK);
    settings_get_str(&reg, "region", buf, sizeof buf);
    CHECK_STR(buf, "US");
    CHECK_EQI(settings_set(&reg, "region", "MARS"), SETTINGS_ERR_ENUM);

    SUITE("settings/groups");
    const char *groups[8];
    int ng = settings_groups(&reg, groups, 8);
    CHECK_EQI(ng, 3);
    CHECK_STR(groups[0], "Device");
    CHECK_STR(groups[1], "Radio");
    CHECK_STR(groups[2], "WiFi");

    SUITE("settings/secret-mask");
    CHECK_EQI(settings_set(&reg, "wifi_pw", "hunter2"), SETTINGS_OK);
    settings_display_value(&reg, "wifi_pw", buf, sizeof buf);
    CHECK(strcmp(buf, "hunter2") != 0);  /* masked, not the real password */
    CHECK(buf[0] != 0);

    SUITE("settings/backcompat-prefilled");
    mem_store_t s2;
    mem_init(&s2);
    mem_set(&s2.base, "tx_power", "9");
    mem_set(&s2.base, "wifi_on", "false");
    settings_t r2;
    settings_init(&r2, &s2.base);
    settings_register_many(&r2, SCHEMA, (int)(sizeof SCHEMA / sizeof SCHEMA[0]));
    CHECK_EQI(settings_get_int(&r2, "tx_power"), 9);
    CHECK(!settings_get_bool(&r2, "wifi_on"));
}
