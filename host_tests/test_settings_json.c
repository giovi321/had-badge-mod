/* Config backup/restore (core/settings_json): round-trip, secret handling,
 * validation, unknown keys, malformed input, escaping, overflow. */
#include "test_util.h"
#include "core/settings.h"
#include "core/settings_json.h"
#include <string.h>

/* in-memory KV store (same shape as test_settings.c) */
typedef struct {
    settings_store_t base;
    char keys[32][32];
    char vals[32][160];
    int n;
} mem_store_t;

static int mget(settings_store_t *self, const char *k, char *b, int cap)
{
    mem_store_t *m = (mem_store_t *)self;
    for (int i = 0; i < m->n; i++)
        if (strcmp(m->keys[i], k) == 0) {
            int j = 0;
            for (; m->vals[i][j] && j < cap - 1; j++) b[j] = m->vals[i][j];
            b[j] = 0;
            return j;
        }
    return -1;
}
static int mset(settings_store_t *self, const char *k, const char *v)
{
    mem_store_t *m = (mem_store_t *)self;
    for (int i = 0; i < m->n; i++)
        if (strcmp(m->keys[i], k) == 0) { strncpy(m->vals[i], v, 159); m->vals[i][159] = 0; return 0; }
    if (m->n >= 32) return -1;
    strncpy(m->keys[m->n], k, 31);
    strncpy(m->vals[m->n], v, 159);
    m->n++;
    return 0;
}
static void mflush(settings_store_t *self) { (void)self; }

static const char *REGION_CHOICES[] = {"EU_868", "US"};
static const setting_t SCHEMA[] = {
    {.key = "device_name", .type = SET_STR,  .def = "Badge-0000", .label = "Name",    .group = "Device"},
    {.key = "tx_power",    .type = SET_INT,  .def = "9",          .label = "TX",      .group = "Radio",
     .minv = 0, .maxv = 22, .has_min = true, .has_max = true},
    {.key = "region",      .type = SET_ENUM, .def = "EU_868",     .label = "Region",  .group = "Radio",
     .choices = REGION_CHOICES, .nchoices = 2},
    {.key = "wifi_pw",     .type = SET_STR,  .def = "",           .label = "WiFi pw", .group = "WiFi", .secret = true},
};
#define NSCHEMA ((int)(sizeof SCHEMA / sizeof SCHEMA[0]))

static void setup(mem_store_t *store, settings_t *reg)
{
    memset(store, 0, sizeof *store);
    store->base.get = mget;
    store->base.set = mset;
    store->base.flush = mflush;
    settings_init(reg, &store->base);
    settings_register_many(reg, SCHEMA, NSCHEMA);
}

void run_settings_json(void)
{
    char json[1024], buf[64];

    SUITE("settings_json/export-roundtrip");
    mem_store_t s1; settings_t r1; setup(&s1, &r1);
    settings_set(&r1, "device_name", "My Badge");
    settings_set(&r1, "tx_power", "17");
    settings_set(&r1, "region", "US");
    settings_set(&r1, "wifi_pw", "hunter2");
    int len = settings_export_json(&r1, true, json, sizeof json);
    CHECK(len > 0);
    CHECK_EQI((int)strlen(json), len);
    CHECK(strstr(json, "\"device_name\":\"My Badge\"") != NULL);
    CHECK(strstr(json, "\"wifi_pw\":\"hunter2\"") != NULL);   /* secret included */

    mem_store_t s2; settings_t r2; setup(&s2, &r2);
    settings_import_result_t res = settings_import_json(&r2, json);
    CHECK_EQI(res.applied, NSCHEMA);
    CHECK_EQI(res.rejected, 0);
    settings_get_str(&r2, "device_name", buf, sizeof buf); CHECK_STR(buf, "My Badge");
    CHECK_EQI(settings_get_int(&r2, "tx_power"), 17);
    settings_get_str(&r2, "region", buf, sizeof buf); CHECK_STR(buf, "US");
    settings_get_str(&r2, "wifi_pw", buf, sizeof buf); CHECK_STR(buf, "hunter2");

    SUITE("settings_json/omit-secrets");
    int len2 = settings_export_json(&r1, false, json, sizeof json);
    CHECK(len2 > 0);
    CHECK(strstr(json, "\"wifi_pw\":null") != NULL);
    CHECK(strstr(json, "hunter2") == NULL);
    mem_store_t s3; settings_t r3; setup(&s3, &r3);
    settings_set(&r3, "wifi_pw", "keepme");
    settings_import_result_t res3 = settings_import_json(&r3, json);
    CHECK(res3.skipped >= 1);
    settings_get_str(&r3, "wifi_pw", buf, sizeof buf); CHECK_STR(buf, "keepme");

    SUITE("settings_json/validation");
    mem_store_t s4; settings_t r4; setup(&s4, &r4);
    settings_import_result_t res4 = settings_import_json(&r4,
        "{\"schema\":1,\"settings\":{\"tx_power\":\"99\",\"device_name\":\"ok\",\"region\":\"MARS\"}}");
    CHECK_EQI(res4.applied, 1);     /* device_name */
    CHECK_EQI(res4.rejected, 2);    /* tx_power range, region not a choice */
    settings_get_str(&r4, "device_name", buf, sizeof buf); CHECK_STR(buf, "ok");

    SUITE("settings_json/unknown-key");
    mem_store_t s5; settings_t r5; setup(&s5, &r5);
    settings_import_result_t res5 = settings_import_json(&r5,
        "{\"settings\":{\"bogus\":\"x\",\"device_name\":\"y\"}}");
    CHECK_EQI(res5.unknown, 1);
    CHECK_EQI(res5.applied, 1);

    SUITE("settings_json/malformed");
    mem_store_t s6; settings_t r6; setup(&s6, &r6);
    settings_import_result_t res6 = settings_import_json(&r6, "not json at all");
    CHECK_EQI(res6.applied, 0);
    CHECK_EQI(res6.rejected, 0);
    settings_import_json(&r6, "{\"settings\":{\"device_name\":\"untermin");   /* no crash */

    SUITE("settings_json/escapes");
    mem_store_t s7; settings_t r7; setup(&s7, &r7);
    settings_set(&r7, "device_name", "a\"b\\c");
    int len7 = settings_export_json(&r7, true, json, sizeof json);
    CHECK(len7 > 0);
    CHECK(strstr(json, "a\\\"b\\\\c") != NULL);   /* escaped in JSON */
    mem_store_t s8; settings_t r8; setup(&s8, &r8);
    settings_import_json(&r8, json);
    settings_get_str(&r8, "device_name", buf, sizeof buf); CHECK_STR(buf, "a\"b\\c");

    SUITE("settings_json/overflow");
    char tiny[16];
    CHECK_EQI(settings_export_json(&r1, true, tiny, sizeof tiny), -1);
}
