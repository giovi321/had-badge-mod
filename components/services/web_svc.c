/* See services/web.h. A small esp_http_server serving a schema-driven settings
 * form and a diagnostics page. */
#include "services/web.h"
#include "services/ota.h"
#include "core/settings_json.h"
#include "net/backend.h"
#include "net/message.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"

static const char *TAG = "web";
static httpd_handle_t s_httpd;
static settings_t *s_reg;

static const char *PAGE_HEAD =
    "<!doctype html><html><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Communicator</title><style>"
    "body{font-family:system-ui,sans-serif;max-width:680px;margin:1rem auto;padding:0 1rem;"
    "background:#0e1116;color:#f2f5f8}h1{font-size:1.3rem}h2{font-size:1rem;color:#e39810;"
    "margin-top:1.5rem;border-bottom:1px solid #2a323d;padding-bottom:.2rem}"
    "label{display:block;margin:.6rem 0 .15rem;color:#93a0ae;font-size:.85rem}"
    "input,select{width:100%;padding:.4rem;background:#171c24;color:#f2f5f8;"
    "border:1px solid #2a323d;border-radius:6px}"
    "button{margin-top:1rem;padding:.5rem 1rem;background:#e39810;color:#1a1206;"
    "border:0;border-radius:6px;font-weight:600}a{color:#e39810}</style></head><body>";

static void chunk(httpd_req_t *r, const char *s) { httpd_resp_sendstr_chunk(r, s); }

static void render_input(httpd_req_t *r, const setting_t *s)
{
    char val[160], buf[640];
    settings_get_str(s_reg, s->key, val, sizeof val);

    snprintf(buf, sizeof buf, "<label>%s</label>", s->label);
    chunk(r, buf);

    if (s->type == SET_BOOL) {
        int on = (strcmp(val, "true") == 0);
        snprintf(buf, sizeof buf,
                 "<select name='%s'><option value='true'%s>on</option>"
                 "<option value='false'%s>off</option></select>",
                 s->key, on ? " selected" : "", on ? "" : " selected");
        chunk(r, buf);
    } else if (s->type == SET_ENUM && s->choices) {
        snprintf(buf, sizeof buf, "<select name='%s'>", s->key);
        chunk(r, buf);
        for (int i = 0; i < s->nchoices; i++) {
            snprintf(buf, sizeof buf, "<option%s>%s</option>",
                     strcmp(s->choices[i], val) == 0 ? " selected" : "", s->choices[i]);
            chunk(r, buf);
        }
        chunk(r, "</select>");
    } else {
        const char *type = (s->type == SET_INT) ? "number" : (s->secret ? "password" : "text");
        snprintf(buf, sizeof buf, "<input type='%s' name='%s' value='%s'>", type, s->key, val);
        chunk(r, buf);
    }
}

static esp_err_t root_get(httpd_req_t *r)
{
    httpd_resp_set_type(r, "text/html");
    chunk(r, PAGE_HEAD);
    chunk(r, "<h1>Communicator settings</h1>"
             "<p><a href='/diag'>diagnostics</a> &middot; "
             "<a href='/import'>backup / restore</a> &middot; "
             "<a href='/update'>firmware update</a></p>"
             "<form method='post' action='/save'>");

    const char *groups[24];
    int ng = settings_groups(s_reg, groups, 24);
    for (int g = 0; g < ng; g++) {
        char h[64];
        snprintf(h, sizeof h, "<h2>%s</h2>", groups[g]);
        chunk(r, h);
        for (int i = 0; i < s_reg->count; i++) {
            const setting_t *s = s_reg->items[i];
            if (strcmp(s->group, groups[g]) == 0) render_input(r, s);
        }
    }
    chunk(r, "<button type=submit>Save</button></form>"
             "<p style='color:#93a0ae;font-size:.8rem'>Radio changes apply after a reboot.</p>"
             "</body></html>");
    httpd_resp_send_chunk(r, NULL, 0);
    return ESP_OK;
}

static void url_decode(char *s)
{
    char *o = s;
    for (char *p = s; *p; p++) {
        if (*p == '+') *o++ = ' ';
        else if (*p == '%' && p[1] && p[2]) {
            int hi = (p[1] <= '9') ? p[1] - '0' : (p[1] | 0x20) - 'a' + 10;
            int lo = (p[2] <= '9') ? p[2] - '0' : (p[2] | 0x20) - 'a' + 10;
            *o++ = (char)((hi << 4) | lo);
            p += 2;
        } else *o++ = *p;
    }
    *o = 0;
}

static esp_err_t save_post(httpd_req_t *r)
{
    char *body = malloc(2048);
    if (!body) return ESP_ERR_NO_MEM;
    int total = 0, got;
    while (total < 2047 && (got = httpd_req_recv(r, body + total, 2047 - total)) > 0) total += got;
    body[total] = 0;

    char val[160];
    for (int i = 0; i < s_reg->count; i++) {
        const setting_t *s = s_reg->items[i];
        if (httpd_query_key_value(body, s->key, val, sizeof val) == ESP_OK) {
            url_decode(val);
            settings_set(s_reg, s->key, val);
        }
    }
    free(body);
    net_reload_config();

    httpd_resp_set_type(r, "text/html");
    chunk(r, PAGE_HEAD);
    chunk(r, "<h1>Saved</h1><p>Reboot for radio changes to take effect.</p>"
             "<p><a href='/'>back to settings</a></p></body></html>");
    httpd_resp_send_chunk(r, NULL, 0);
    return ESP_OK;
}

static esp_err_t diag_get(httpd_req_t *r)
{
    net_diag_t d;
    net_diag(&d);
    char id[12];
    net_node_id_str(d.node, id);

    int fmhz = (int)d.freq_mhz;
    int fkhz = (int)((d.freq_mhz - fmhz) * 1000.0 + 0.5);
    char buf[700];
    snprintf(buf, sizeof buf,
             "<h1>Diagnostics</h1><table>"
             "<tr><td>Node</td><td>%s</td></tr>"
             "<tr><td>Radio</td><td>%s %s %d.%03d MHz SF%d</td></tr>"
             "<tr><td>Channel</td><td>%s (sync 0x%02X)</td></tr>"
             "<tr><td>Hops</td><td>limit %d, relay %s</td></tr>"
             "<tr><td>Peers</td><td>%d</td></tr>"
             "<tr><td>RX/TX</td><td>%lu / %lu</td></tr>"
             "<tr><td>Last signal</td><td>RSSI %d, SNR %d</td></tr>"
             "</table><p><a href='/'>settings</a></p></body></html>",
             id, d.region, d.preset, fmhz, fkhz, d.sf, d.channel, d.sync_word,
             d.hop_limit, d.relay ? "on" : "off", d.peers,
             (unsigned long)d.rx_count, (unsigned long)d.tx_count,
             (int)d.last_rssi, (int)d.last_snr);
    httpd_resp_set_type(r, "text/html");
    chunk(r, PAGE_HEAD);
    chunk(r, buf);
    httpd_resp_send_chunk(r, NULL, 0);
    return ESP_OK;
}

/* GET /export -> downloads the full config as JSON. ?secrets=0 omits secrets. */
static esp_err_t export_get(httpd_req_t *r)
{
    bool secrets = true;
    char q[48], v[8];
    if (httpd_req_get_url_query_str(r, q, sizeof q) == ESP_OK &&
        httpd_query_key_value(q, "secrets", v, sizeof v) == ESP_OK && strcmp(v, "0") == 0)
        secrets = false;

    int cap = 12 * 1024;
    char *buf = malloc(cap);
    if (!buf) return ESP_ERR_NO_MEM;
    int n = settings_export_json(s_reg, secrets, buf, cap);
    if (n < 0) { free(buf); httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "config too large"); return ESP_FAIL; }

    httpd_resp_set_type(r, "application/json");
    httpd_resp_set_hdr(r, "Content-Disposition", "attachment; filename=\"badge-config.json\"");
    httpd_resp_send(r, buf, n);
    free(buf);
    return ESP_OK;
}

/* GET /import -> a page to download a backup or paste one to restore. */
static esp_err_t import_get(httpd_req_t *r)
{
    httpd_resp_set_type(r, "text/html");
    chunk(r, PAGE_HEAD);
    chunk(r, "<h1>Backup / restore</h1>"
             "<p><a href='/export'>Download backup</a> (includes channel key and WiFi password) "
             "&middot; <a href='/export?secrets=0'>without secrets</a></p>"
             "<form method='post' action='/import'>"
             "<label>Paste a badge-config.json to restore</label>"
             "<textarea name='cfg' rows='10' style='width:100%'></textarea>"
             "<button type=submit>Restore</button></form>"
             "<p><a href='/'>back to settings</a></p></body></html>");
    httpd_resp_send_chunk(r, NULL, 0);
    return ESP_OK;
}

/* POST /import -> applies a pasted backup (form field cfg=<json>). */
static esp_err_t import_post(httpd_req_t *r)
{
    int cap = 16 * 1024;
    char *body = malloc(cap);
    if (!body) return ESP_ERR_NO_MEM;
    int total = 0, got;
    while (total < cap - 1 && (got = httpd_req_recv(r, body + total, cap - 1 - total)) > 0) total += got;
    body[total] = 0;

    char *p = body;
    if (strncmp(p, "cfg=", 4) == 0) p += 4;
    url_decode(p);
    settings_import_result_t res = settings_import_json(s_reg, p);
    free(body);
    net_reload_config();

    char msg[256];
    snprintf(msg, sizeof msg,
             "<h1>Restored</h1><p>%d applied, %d skipped, %d rejected, %d unknown.</p>"
             "<p>Reboot for radio/WiFi changes to take effect.</p>"
             "<p><a href='/'>settings</a></p></body></html>",
             res.applied, res.skipped, res.rejected, res.unknown);
    httpd_resp_set_type(r, "text/html");
    chunk(r, PAGE_HEAD);
    chunk(r, msg);
    httpd_resp_send_chunk(r, NULL, 0);
    return ESP_OK;
}

/* GET /update -> a file picker that POSTs the raw .bin as the request body. */
static esp_err_t update_get(httpd_req_t *r)
{
    httpd_resp_set_type(r, "text/html");
    chunk(r, PAGE_HEAD);
    chunk(r, "<h1>Firmware update</h1>"
             "<p>Pick a had-badge-mod <code>.bin</code> built for this board. The badge "
             "reboots into the new image; a bad image rolls back on the next reset.</p>"
             "<input type=file id=f accept='.bin'>"
             "<button onclick='up()'>Flash</button><p id=s></p>"
             "<script>function up(){var f=document.getElementById('f').files[0];if(!f)return;"
             "var s=document.getElementById('s');s.textContent='Uploading '+f.size+' bytes...';"
             "fetch('/update',{method:'POST',body:f}).then(function(r){return r.text()})"
             ".then(function(t){s.textContent=t}).catch(function(e){s.textContent='Error: '+e});}"
             "</script><p><a href='/'>back</a></p></body></html>");
    httpd_resp_send_chunk(r, NULL, 0);
    return ESP_OK;
}

/* POST /update -> streams the request body straight into the inactive OTA slot. */
static esp_err_t update_post(httpd_req_t *r)
{
    int total = r->content_len;
    if (ota_begin((size_t)(total > 0 ? total : 0)) != ESP_OK) {
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "ota begin failed");
        return ESP_FAIL;
    }

    char *buf = malloc(4096);
    if (!buf) { ota_abort(); return ESP_ERR_NO_MEM; }
    int remaining = total, got;
    bool ok = true;
    while (remaining > 0) {
        got = httpd_req_recv(r, buf, remaining < 4096 ? remaining : 4096);
        if (got == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (got <= 0 || ota_write(buf, (size_t)got) != ESP_OK) { ok = false; break; }
        remaining -= got;
    }
    free(buf);

    if (!ok || remaining > 0) {
        ota_abort();
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "upload failed");
        return ESP_FAIL;
    }
    if (ota_end() != ESP_OK) {
        httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "image rejected");
        return ESP_FAIL;
    }
    httpd_resp_sendstr(r, "Update OK. Rebooting into the new firmware...");
    ota_reboot_soon();
    return ESP_OK;
}

esp_err_t web_svc_start(settings_t *reg)
{
    if (s_httpd) return ESP_OK;
    s_reg = reg;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.lru_purge_enable = true;
    cfg.max_uri_handlers = 12;
    cfg.stack_size = 6144;
    esp_err_t e = httpd_start(&s_httpd, &cfg);
    if (e != ESP_OK) { ESP_LOGE(TAG, "httpd start: %s", esp_err_to_name(e)); return e; }

    httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = root_get };
    httpd_uri_t save = { .uri = "/save", .method = HTTP_POST, .handler = save_post };
    httpd_uri_t diag = { .uri = "/diag", .method = HTTP_GET, .handler = diag_get };
    httpd_uri_t expo = { .uri = "/export", .method = HTTP_GET, .handler = export_get };
    httpd_uri_t impg = { .uri = "/import", .method = HTTP_GET, .handler = import_get };
    httpd_uri_t impp = { .uri = "/import", .method = HTTP_POST, .handler = import_post };
    httpd_uri_t updg = { .uri = "/update", .method = HTTP_GET, .handler = update_get };
    httpd_uri_t updp = { .uri = "/update", .method = HTTP_POST, .handler = update_post };
    httpd_register_uri_handler(s_httpd, &root);
    httpd_register_uri_handler(s_httpd, &save);
    httpd_register_uri_handler(s_httpd, &diag);
    httpd_register_uri_handler(s_httpd, &expo);
    httpd_register_uri_handler(s_httpd, &impg);
    httpd_register_uri_handler(s_httpd, &impp);
    httpd_register_uri_handler(s_httpd, &updg);
    httpd_register_uri_handler(s_httpd, &updp);
    ESP_LOGI(TAG, "web ui started");
    return ESP_OK;
}

void web_svc_stop(void)
{
    if (s_httpd) { httpd_stop(s_httpd); s_httpd = NULL; }
}
