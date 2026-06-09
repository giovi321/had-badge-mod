/* Tiny zero-dependency test harness for the portable firmware logic.
 * Each suite is a void run_*(void) that uses the CHECK_* macros below; the
 * shared counters live in test_main.c. Build all suites into one executable. */
#ifndef HT_TEST_UTIL_H
#define HT_TEST_UTIL_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>

extern int ht_checks;
extern int ht_fails;
extern const char *ht_cur;

#define SUITE(name) (ht_cur = (name))

#define CHECK(cond) do { \
    ht_checks++; \
    if (!(cond)) { ht_fails++; \
        printf("  [FAIL] %s @ %s:%d: %s\n", ht_cur, __FILE__, __LINE__, #cond); } \
} while (0)

#define CHECK_EQI(a, b) do { \
    ht_checks++; \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (_a != _b) { ht_fails++; \
        printf("  [FAIL] %s @ %s:%d: (%s)=%lld != (%s)=%lld\n", \
               ht_cur, __FILE__, __LINE__, #a, _a, #b, _b); } \
} while (0)

#define CHECK_MEM(a, b, n) do { \
    ht_checks++; \
    if (memcmp((a), (b), (size_t)(n)) != 0) { ht_fails++; \
        printf("  [FAIL] %s @ %s:%d: mem %s != %s (%d bytes)\n", \
               ht_cur, __FILE__, __LINE__, #a, #b, (int)(n)); } \
} while (0)

#define CHECK_STR(a, b) do { \
    ht_checks++; \
    if (strcmp((a), (b)) != 0) { ht_fails++; \
        printf("  [FAIL] %s @ %s:%d: \"%s\" != \"%s\"\n", \
               ht_cur, __FILE__, __LINE__, (a), (b)); } \
} while (0)

#define CHECK_NEAR(a, b, eps) do { \
    ht_checks++; \
    double _d = (double)(a) - (double)(b); if (_d < 0) _d = -_d; \
    if (_d > (double)(eps)) { ht_fails++; \
        printf("  [FAIL] %s @ %s:%d: |%s-%s|=%g > %g\n", \
               ht_cur, __FILE__, __LINE__, #a, #b, _d, (double)(eps)); } \
} while (0)

/* Parse a hex string ("00112233...") into bytes. Returns the byte count. */
static inline int ht_hex(const char *hex, uint8_t *out, int out_cap)
{
    int n = 0;
    for (const char *p = hex; p[0] && p[1]; p += 2) {
        if (n >= out_cap) break;
        int hi, lo;
        char c = p[0];
        hi = (c <= '9') ? c - '0' : (c | 0x20) - 'a' + 10;
        c = p[1];
        lo = (c <= '9') ? c - '0' : (c | 0x20) - 'a' + 10;
        out[n++] = (uint8_t)((hi << 4) | lo);
    }
    return n;
}

#endif /* HT_TEST_UTIL_H */
