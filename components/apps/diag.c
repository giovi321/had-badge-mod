/* Diagnostics app: per-module status (LoRa mesh, GPS, WiFi, Bluetooth, system).
 *
 * The "Heard" row under LoRa counts every valid LoRa frame the radio demodulates
 * on any channel, before the channel/decrypt filter. It is the key field when
 * messages are not getting through: if it climbs, the radio hears traffic. */
#include "apps/app_iface.h"
#include "ui/frame.h"
#include "ui/theme.h"
#include "ui/colors.h"
#include "ui/menubar.h"
#include "net/backend.h"
#include "net/message.h"
#include "drivers/gps.h"
#include "drivers/wifi.h"
#include "drivers/battery.h"
#include "ble/ble.h"

#include <stdio.h>
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_system.h"

enum {
    R_NODE, R_RADIO, R_CHAN, R_RXTX, R_HEARD, R_SIG, R_PEERS,
    R_GPS, R_GPS_POS,
    R_WIFI, R_WIFI_IP,
    R_BLE,
    R_BATT, R_SYS,
    R_COUNT
};
static lv_obj_t *s_val[R_COUNT];

static void make_header(lv_obj_t *parent, const char *caps)
{
    lv_obj_t *h = lv_label_create(parent);
    lv_label_set_text(h, caps);
    lv_obj_set_style_text_color(h, theme_hex(C_ACCENT), 0);
    lv_obj_set_style_text_letter_space(h, 1, 0);
    lv_obj_set_style_pad_top(h, 3, 0);
}

static lv_obj_t *make_row(lv_obj_t *parent, const char *caption)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_width(box, LV_PCT(100));
    lv_obj_set_height(box, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 1, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *cap = lv_label_create(box);
    lv_label_set_text(cap, caption);
    lv_obj_set_width(cap, 64);
    lv_obj_set_style_text_color(cap, theme_hex(C_TEXT_DIM), 0);
    lv_obj_t *val = lv_label_create(box);
    lv_label_set_long_mode(val, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(val, 1);
    lv_obj_set_style_text_color(val, theme_hex(C_TEXT), 0);
    lv_label_set_text(val, "--");
    return val;
}

static void build(lv_obj_t **screen, lv_group_t *group)
{
    static frame_t f;
    frame_create(&f, "Diagnostics");
    lv_obj_set_flex_flow(f.body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(f.body, 1, 0);
    ui_scroll_focusable(f.body, group);

    make_header(f.body, "LORA MESH");
    s_val[R_NODE]  = make_row(f.body, "Node");
    s_val[R_RADIO] = make_row(f.body, "Radio");
    s_val[R_CHAN]  = make_row(f.body, "Channel");
    s_val[R_RXTX]  = make_row(f.body, "RX/TX");
    s_val[R_HEARD] = make_row(f.body, "Heard");
    s_val[R_SIG]   = make_row(f.body, "Signal");
    s_val[R_PEERS] = make_row(f.body, "Peers");

    make_header(f.body, "GPS");
    s_val[R_GPS]     = make_row(f.body, "Fix");
    s_val[R_GPS_POS] = make_row(f.body, "Pos");

    make_header(f.body, "WIFI");
    s_val[R_WIFI]    = make_row(f.body, "State");
    s_val[R_WIFI_IP] = make_row(f.body, "IP");

    make_header(f.body, "BLUETOOTH");
    s_val[R_BLE] = make_row(f.body, "State");

    make_header(f.body, "SYSTEM");
    s_val[R_BATT] = make_row(f.body, "Battery");
    s_val[R_SYS]  = make_row(f.body, "Up/Heap");

    *screen = f.screen;
    menubar_set_labels("", "", "", "", "");
}

static void tick(void)
{
    char b[96], id[12];

    /* --- LoRa mesh --- */
    net_diag_t d;
    net_diag(&d);
    net_node_id_str(d.node, id);
    lv_label_set_text(s_val[R_NODE], id);
    snprintf(b, sizeof b, "%s %.3fMHz SF%d", d.region, d.freq_mhz, d.sf);
    lv_label_set_text(s_val[R_RADIO], b);
    snprintf(b, sizeof b, "%s  sync 0x%02X", d.channel, d.sync_word);
    lv_label_set_text(s_val[R_CHAN], b);
    snprintf(b, sizeof b, "%lu / %lu", (unsigned long)d.rx_count, (unsigned long)d.tx_count);
    lv_label_set_text(s_val[R_RXTX], b);
    snprintf(b, sizeof b, "%lu frames (any ch)", (unsigned long)d.rx_raw);
    lv_label_set_text(s_val[R_HEARD], b);
    snprintf(b, sizeof b, "RSSI %.0f  SNR %.1f", (double)d.last_rssi, (double)d.last_snr);
    lv_label_set_text(s_val[R_SIG], b);
    snprintf(b, sizeof b, "%d", d.peers);
    lv_label_set_text(s_val[R_PEERS], b);

    /* --- GPS --- */
    gps_fix_t fix;
    bool havefix = gps_get_fix(&fix) && fix.valid;
    if (havefix) {
        snprintf(b, sizeof b, "fix, %d sats", fix.sats);
        lv_label_set_text(s_val[R_GPS], b);
        snprintf(b, sizeof b, "%.5f, %.5f", fix.lat, fix.lon);
        lv_label_set_text(s_val[R_GPS_POS], b);
    } else {
        lv_label_set_text(s_val[R_GPS], "no fix");
        lv_label_set_text(s_val[R_GPS_POS], "--");
    }

    /* --- WiFi --- */
    char st[24]; int wr = 0; bool wrv = false;
    wifi_get_state(st, sizeof st, &wr, &wrv);
    if (wrv) snprintf(b, sizeof b, "%s  %d dBm", st, wr);
    else snprintf(b, sizeof b, "%s", st);
    lv_label_set_text(s_val[R_WIFI], b);
    if (wifi_link_up()) { char ip[20]; wifi_ip_str(ip, sizeof ip); lv_label_set_text(s_val[R_WIFI_IP], ip); }
    else lv_label_set_text(s_val[R_WIFI_IP], "--");

    /* --- Bluetooth --- */
    bool ben = false, bcon = false;
    ble_status(&ben, &bcon);
    lv_label_set_text(s_val[R_BLE], !ben ? "off" : (bcon ? "connected" : "advertising"));

    /* --- System --- */
    battery_state_t bat;
    if (battery_read(&bat) && bat.present)
        snprintf(b, sizeof b, "%d%%  %.2fV%s", bat.pct, (double)bat.volts, bat.usb ? " USB" : "");
    else
        snprintf(b, sizeof b, "disabled");
    lv_label_set_text(s_val[R_BATT], b);
    uint32_t up = (uint32_t)(esp_timer_get_time() / 1000000);
    snprintf(b, sizeof b, "%lum%02lus  %luK", (unsigned long)(up / 60),
             (unsigned long)(up % 60), (unsigned long)(esp_get_free_heap_size() / 1024));
    lv_label_set_text(s_val[R_SYS], b);
}

const app_def_t *app_diag(void)
{
    static const app_def_t def = {
        .name = "Diag", .icon = LV_SYMBOL_EYE_OPEN,
        .build = build, .tick = tick,
    };
    return &def;
}
