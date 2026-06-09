/* See util/nmea.h. */
#include "util/nmea.h"
#include <string.h>
#include <stdlib.h>

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)(c | 0x20);
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static bool checksum_ok(const char *s)
{
    const char *star = strrchr(s, '*');
    if (!star) return false;
    int hi = hexval(star[1]), lo = star[2] ? hexval(star[2]) : -1;
    if (hi < 0 || lo < 0) return false;
    int want = (hi << 4) | lo;
    int got = 0;
    for (const char *p = s; p < star; p++) got ^= (unsigned char)*p;
    return got == want;
}

static bool all_digits(const char *s, int n)
{
    for (int i = 0; i < n; i++)
        if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

static int two(const char *s) { return (s[0] - '0') * 10 + (s[1] - '0'); }

bool nmea_dm_to_deg(const char *value, char hemi, double *out)
{
    if (!value || !*value) return false;
    const char *dotp = strchr(value, '.');
    if (!dotp) return false;
    int dot = (int)(dotp - value);
    if (dot < 3) return false;
    int dn = dot - 2;
    char degbuf[8];
    if (dn <= 0 || dn >= (int)sizeof(degbuf)) return false;
    memcpy(degbuf, value, (size_t)dn);
    degbuf[dn] = 0;
    int deg = atoi(degbuf);
    double minutes = atof(value + dn);
    double dec = deg + minutes / 60.0;
    if (hemi == 'S' || hemi == 'W') dec = -dec;
    *out = dec;
    return true;
}

uint32_t nmea_to_unix(const char *date, const char *tm)
{
    if (!date || !tm || strlen(date) < 6 || strlen(tm) < 6) return 0;
    if (!all_digits(date, 6) || !all_digits(tm, 6)) return 0;
    int day = two(date), month = two(date + 2), year = 2000 + two(date + 4);
    int hh = two(tm), mm = two(tm + 2), ss = (int)atof(tm + 4);

    int y = year - (month <= 2 ? 1 : 0);
    int era = (y >= 0 ? y : y - 399) / 400;
    int yoe = y - era * 400;
    int doy = (153 * ((month + (month > 2 ? -3 : 9)) % 12) + 2) / 5 + (day - 1);
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long days = (long)era * 146097 + doe - 719468;
    return (uint32_t)(days * 86400L + hh * 3600 + mm * 60 + ss);
}

static void fill_datetime(nmea_result_t *r, const char *date, const char *tm)
{
    if (!date || !tm || strlen(date) < 6 || strlen(tm) < 6 ||
        !all_digits(date, 6) || !all_digits(tm, 6)) {
        r->has_datetime = false;
        return;
    }
    r->has_datetime = true;
    r->year = 2000 + two(date + 4);
    r->mon = two(date + 2);
    r->day = two(date);
    r->hh = two(tm);
    r->mm = two(tm + 2);
    r->ss = (int)atof(tm + 4);
}

bool nmea_parse_sentence(const char *sentence, nmea_result_t *out)
{
    memset(out, 0, sizeof(*out));
    if (!checksum_ok(sentence)) return false;

    char buf[100];
    size_t n = strlen(sentence);
    const char *star = strrchr(sentence, '*');
    size_t body = star ? (size_t)(star - sentence) : n;
    if (body >= sizeof(buf)) body = sizeof(buf) - 1;
    memcpy(buf, sentence, body);
    buf[body] = 0;

    char *f[26];
    int nf = 0;
    f[nf++] = buf;
    for (char *p = buf; *p; p++) {
        if (*p == ',') { *p = 0; if (nf < 26) f[nf++] = p + 1; }
    }

    const char *talker = f[0];
    const char *kind = (strlen(talker) >= 5) ? talker + 2 : talker;

    if (strcmp(kind, "RMC") == 0 && nf >= 10) {
        out->kind = NMEA_RMC;
        out->has_lat = nmea_dm_to_deg(f[3], f[4][0], &out->lat);
        out->has_lon = nmea_dm_to_deg(f[5], f[6][0], &out->lon);
        out->speed = f[7][0] ? atof(f[7]) : 0.0;
        if (f[8][0]) { out->has_track = true; out->track = atof(f[8]); }
        out->ts = nmea_to_unix(f[9], f[1]);
        fill_datetime(out, f[9], f[1]);
        out->valid = (strcmp(f[2], "A") == 0) && out->has_lat && out->has_lon;
        return true;
    }
    if (strcmp(kind, "GGA") == 0 && nf >= 10) {
        out->kind = NMEA_GGA;
        out->has_lat = nmea_dm_to_deg(f[2], f[3][0], &out->lat);
        out->has_lon = nmea_dm_to_deg(f[4], f[5][0], &out->lon);
        out->quality = f[6][0] ? atoi(f[6]) : 0;
        out->sats = f[7][0] ? atoi(f[7]) : 0;
        out->alt = f[9][0] ? (int32_t)atof(f[9]) : 0;
        out->valid = (out->quality > 0) && out->has_lat && out->has_lon;
        return true;
    }
    return false;
}

void nmea_parser_init(nmea_parser_t *p) { p->len = 0; p->buf[0] = 0; }

int nmea_parser_feed(nmea_parser_t *p, const char *data, int n, nmea_cb_t cb, void *ctx)
{
    int emitted = 0;
    for (int i = 0; i < n; i++) {
        char c = data[i];
        if (c == '\n') {
            p->buf[p->len] = 0;
            /* strip trailing \r and spaces */
            int e = p->len;
            while (e > 0 && (p->buf[e - 1] == '\r' || p->buf[e - 1] == ' ')) p->buf[--e] = 0;
            int s = 0;
            while (p->buf[s] == ' ') s++;
            if (p->buf[s] == '$') {
                nmea_result_t r;
                if (nmea_parse_sentence(p->buf + s + 1, &r)) {
                    if (cb) cb(&r, ctx);
                    emitted++;
                }
            }
            p->len = 0;
        } else if (p->len < (int)sizeof(p->buf) - 1) {
            p->buf[p->len++] = c;
        } else {
            /* overlong line: keep the tail, drop the head */
            memmove(p->buf, p->buf + p->len - 100, 100);
            p->len = 100;
            p->buf[p->len++] = c;
        }
    }
    return emitted;
}
