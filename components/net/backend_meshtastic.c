/* See net/backend_meshtastic.h. Ported from net/mesh/backend_meshtastic.py. */
#include "net/backend_meshtastic.h"
#include "net/dedup.h"
#include "mesh/packet.h"
#include "mesh/mesh_crypto.h"
#include "mesh/regions.h"
#include "mesh/meshtastic_pb.h"

#include <string.h>
#include <stdio.h>
#include "esp_random.h"
#include "esp_log.h"

static const char *TAG = "meshtastic";

static const char *REGION_CHOICES[] = {
    "US", "EU_868", "EU_433", "ANZ", "AU_915", "IN", "JP",
    "KR", "CN", "RU", "TW", "TH", "UA_868"
};
static const char *PRESET_CHOICES[] = {
    "ShortTurbo", "ShortFast", "ShortSlow", "MediumFast", "MediumSlow",
    "LongFast", "LongModerate", "LongSlow", "VeryLongSlow"
};
static const char *BACKEND_CHOICES[] = { "meshtastic" };

static const setting_t MESH_SCHEMA[] = {
    {.key = "net_backend", .type = SET_ENUM, .def = "meshtastic", .label = "Network stack",
     .group = "Network", .choices = BACKEND_CHOICES, .nchoices = 1},
    {.key = "mesh_region", .type = SET_ENUM, .def = "EU_868", .label = "LoRa region",
     .group = "Radio", .choices = REGION_CHOICES, .nchoices = 13},
    {.key = "mesh_preset", .type = SET_ENUM, .def = "LongFast", .label = "Modem preset",
     .group = "Radio", .choices = PRESET_CHOICES, .nchoices = 9},
    {.key = "mesh_chan", .type = SET_STR, .def = "LongFast", .label = "Channel name", .group = "Radio"},
    {.key = "mesh_psk", .type = SET_STR, .def = "AQ==", .label = "Channel key (b64)", .group = "Radio", .secret = true},
    {.key = "mesh_hop", .type = SET_INT, .def = "3", .label = "Hop limit", .group = "Radio",
     .minv = 0, .maxv = 7, .has_min = true, .has_max = true},
    {.key = "mesh_relay", .type = SET_BOOL, .def = "false", .label = "Relay others (router)", .group = "Radio"},
    {.key = "mesh_share_pos", .type = SET_BOOL, .def = "false", .label = "Share GPS position", .group = "Radio"},
    {.key = "mesh_pos_int", .type = SET_INT, .def = "60", .label = "Position interval s", .group = "Radio",
     .minv = 10, .maxv = 3600, .has_min = true, .has_max = true},
    {.key = "radio_tx_power", .type = SET_INT, .def = "9", .label = "TX power dBm", .group = "Radio",
     .minv = -9, .maxv = 22, .has_min = true, .has_max = true},
    {.key = "device_name", .type = SET_STR, .def = "", .label = "Device name", .group = "Device"},
    {.key = "short_name", .type = SET_STR, .def = "", .label = "Short name", .group = "Device"},
};

static struct {
    settings_t *st;
    eventbus_t *bus;
    uint32_t my_node;
    net_tx_fn_t tx;
    char channel[24];
    uint8_t psk[32];
    int psk_len;
    int hop_limit;
    bool rebroadcast;
    int tx_power;
    char region[16], preset[16];
    double freq, bw;
    int sf, cr, preamble;
    char device_name[48], short_name[16];
    nodedb_t db;
    dedup_t dd;
    uint32_t rx_count, tx_count, rx_raw;
    float last_rssi, last_snr;
    uint32_t ack_pending[16];   /* packet ids of unicast sends awaiting an ack */
    int ack_head;
    net_pkt_log_t pktlog[32];
    int pktlog_n, pktlog_head;
} g;

static void pktlog_add(uint32_t from, uint8_t port, float rssi, float snr, uint32_t when)
{
    net_pkt_log_t *e = &g.pktlog[g.pktlog_head];
    e->from = from; e->portnum = port; e->rssi = (int16_t)rssi; e->snr = snr; e->when = when;
    g.pktlog_head = (g.pktlog_head + 1) % 32;
    if (g.pktlog_n < 32) g.pktlog_n++;
}

int mtb_packet_log(net_pkt_log_t *out, int max)
{
    int n = g.pktlog_n;
    if (n > max) n = max;
    for (int i = 0; i < n; i++)
        out[i] = g.pktlog[(g.pktlog_head - 1 - i + 64) % 32];   /* most recent first */
    return n;
}

static net_rx_observer_fn s_rx_observer;   /* persists across reload */
void mtb_set_rx_observer(net_rx_observer_fn fn) { s_rx_observer = fn; }

bool mtb_send_meshpacket(uint32_t to, bool want_ack, const uint8_t *data, int data_len)
{
    uint8_t frame[256];
    uint32_t pid = esp_random();
    if (!pid) pid = 1;
    int flen = mesh_build_packet(frame, sizeof frame, to, g.my_node, pid, g.channel,
                                 g.psk, (size_t)g.psk_len, data, (size_t)data_len,
                                 g.hop_limit, g.hop_limit, want_ack);
    if (flen < 0 || !g.tx) return false;
    bool ok = g.tx(frame, flen);
    if (ok) g.tx_count++;
    return ok;
}

void mtb_register_settings(settings_t *st)
{
    settings_register_many(st, MESH_SCHEMA, (int)(sizeof MESH_SCHEMA / sizeof MESH_SCHEMA[0]));
}

void mtb_reload(void)
{
    settings_t *st = g.st;
    settings_get_str(st, "mesh_region", g.region, sizeof g.region);
    settings_get_str(st, "mesh_preset", g.preset, sizeof g.preset);
    settings_get_str(st, "mesh_chan", g.channel, sizeof g.channel);

    char b64[48];
    settings_get_str(st, "mesh_psk", b64, sizeof b64);
    int n = mesh_parse_psk_b64(b64, g.psk);
    g.psk_len = (n < 0) ? 1 : n;
    if (n < 0) g.psk[0] = 0x01;          /* fall back to default channel */

    g.hop_limit = (int)settings_get_int(st, "mesh_hop");
    g.rebroadcast = settings_get_bool(st, "mesh_relay");
    g.tx_power = (int)settings_get_int(st, "radio_tx_power");

    settings_get_str(st, "device_name", g.device_name, sizeof g.device_name);
    settings_get_str(st, "short_name", g.short_name, sizeof g.short_name);
    if (!g.device_name[0]) net_node_id_str(g.my_node, g.device_name);
    if (!g.short_name[0]) snprintf(g.short_name, sizeof g.short_name, "%.4s", g.device_name + 1);

    const mesh_preset_t *pr = mesh_preset_find(g.preset);
    const mesh_region_t *rg = mesh_region_find(g.region);
    if (!pr) pr = mesh_preset_find("LongFast");
    if (!rg) rg = mesh_region_find("EU_868");
    g.bw = pr->bw_khz; g.sf = pr->sf; g.cr = pr->cr; g.preamble = pr->preamble;
    g.freq = mesh_center_freq(g.channel, rg, g.bw);

    /* clamp TX power to region cap */
    if (g.tx_power > rg->power_limit_dbm) g.tx_power = rg->power_limit_dbm;
    ESP_LOGI(TAG, "config: %s/%s ch '%s' %.3fMHz SF%d hop%d relay=%d",
             g.region, g.preset, g.channel, g.freq, g.sf, g.hop_limit, g.rebroadcast);
}

void mtb_init(settings_t *st, eventbus_t *bus, uint32_t my_node)
{
    memset(&g, 0, sizeof g);
    g.st = st; g.bus = bus; g.my_node = my_node;
    nodedb_init(&g.db);
    dedup_init(&g.dd);
    mtb_reload();
}

void mtb_set_tx(net_tx_fn_t fn) { g.tx = fn; }

void mtb_radio_cfg(net_radio_cfg_t *out)
{
    out->freq_mhz = g.freq; out->bw_khz = g.bw;
    out->sf = g.sf; out->cr = g.cr; out->sync_word = MESH_SYNC_WORD;
    out->preamble = g.preamble; out->power_dbm = g.tx_power;
}

/* --- TX ----------------------------------------------------------------- */
static bool send_data(int portnum, const uint8_t *payload, int payload_len,
                      uint32_t to_id, bool want_ack, uint32_t *out_pid)
{
    meshtastic_Data d = meshtastic_Data_init_zero;
    d.portnum = (meshtastic_PortNum)portnum;
    if (payload_len > (int)sizeof d.payload.bytes) return false;
    memcpy(d.payload.bytes, payload, (size_t)payload_len);
    d.payload.size = (pb_size_t)payload_len;

    uint8_t data[256];
    int dlen = mt_data_encode(data, sizeof data, &d);
    if (dlen < 0) return false;

    uint8_t frame[256];
    uint32_t pid = esp_random();
    if (!pid) pid = 1;
    int flen = mesh_build_packet(frame, sizeof frame, to_id, g.my_node, pid,
                                 g.channel, g.psk, (size_t)g.psk_len, data, (size_t)dlen,
                                 g.hop_limit, g.hop_limit, want_ack);
    if (flen < 0 || !g.tx) return false;
    bool ok = g.tx(frame, flen);
    if (ok) { g.tx_count++; if (out_pid) *out_pid = pid; }
    return ok;
}

bool mtb_send_text_to(uint32_t to_id, const char *text)
{
    int n = (int)strlen(text);
    if (n > MSG_TEXT_MAX) n = MSG_TEXT_MAX;
    bool unicast = (to_id != MESH_BROADCAST);
    uint32_t pid = 0;
    bool ok = send_data(meshtastic_PortNum_TEXT_MESSAGE_APP, (const uint8_t *)text, n,
                        to_id, unicast, &pid);
    if (ok && unicast) {
        g.ack_pending[g.ack_head] = pid;
        g.ack_head = (g.ack_head + 1) % 16;
    }
    if (ok && g.bus) {
        net_message_t m = {0};
        m.kind = MSG_TEXT; m.from_id = g.my_node; m.to_id = to_id; m.id = pid; m.outgoing = true;
        memcpy(m.text, text, (size_t)n); m.text[n] = 0;
        eventbus_publish(g.bus, EV_MESSAGE_SENT, &m);
    }
    return ok;
}

bool mtb_send_text(const char *text) { return mtb_send_text_to(MESH_BROADCAST, text); }

bool mtb_send_telemetry(int battery_pct, float voltage, uint32_t uptime_s)
{
    meshtastic_Telemetry t = meshtastic_Telemetry_init_zero;
    t.has_device_metrics = true;
    if (battery_pct >= 0) t.device_metrics.battery_level = (uint32_t)battery_pct;
    t.device_metrics.voltage = voltage;
    t.device_metrics.uptime_seconds = uptime_s;
    uint8_t payload[64];
    int pl = mt_telemetry_encode(payload, sizeof payload, &t);
    if (pl < 0) return false;
    return send_data(meshtastic_PortNum_TELEMETRY_APP, payload, pl, MESH_BROADCAST, false, NULL);
}

bool mtb_send_position(double lat, double lon, int32_t alt, uint32_t ts)
{
    meshtastic_Position p = meshtastic_Position_init_zero;
    p.latitude_i = (int32_t)(lat * 1e7);
    p.longitude_i = (int32_t)(lon * 1e7);
    p.altitude = alt;
    p.time = ts;
    uint8_t payload[80];
    int pl = mt_position_encode(payload, sizeof payload, &p);
    if (pl < 0) return false;
    return send_data(meshtastic_PortNum_POSITION_APP, payload, pl, MESH_BROADCAST, false, NULL);
}

bool mtb_send_nodeinfo(void)
{
    meshtastic_User u = meshtastic_User_init_zero;
    net_node_id_str(g.my_node, u.id);
    snprintf(u.long_name, sizeof u.long_name, "%s", g.device_name);
    snprintf(u.short_name, sizeof u.short_name, "%s", g.short_name);
    uint8_t payload[128];
    int pl = mt_user_encode(payload, sizeof payload, &u);
    if (pl < 0) return false;
    return send_data(meshtastic_PortNum_NODEINFO_APP, payload, pl, MESH_BROADCAST, false, NULL);
}

/* --- RX ----------------------------------------------------------------- */
void mtb_on_frame(const uint8_t *frame, int len, float rssi, float snr, uint32_t now)
{
    /* Any valid-CRC LoRa frame the radio demodulated, before the channel/decrypt
     * filter. Lets diagnostics distinguish "hears nothing" from "hears other
     * channels". The signal is the last heard, on any channel. */
    g.rx_raw++;
    g.last_rssi = rssi;
    g.last_snr = snr;

    mesh_header_t hdr;
    uint8_t plain[256];
    size_t pl = 0;
    if (mesh_parse_packet(frame, (size_t)len, g.channel, g.psk, (size_t)g.psk_len,
                          &hdr, plain, sizeof plain, &pl) != 0)
        return;
    if (!dedup_check_and_add(&g.dd, hdr.from, hdr.id, now)) return;
    if (hdr.from == g.my_node) return;   /* our own echo */

    if (mesh_should_rebroadcast(g.rebroadcast, hdr.to, hdr.hop_limit) && g.tx) {
        uint8_t rb[256];
        memcpy(rb, frame, (size_t)len);
        if (mesh_decrement_hop(rb, (size_t)len, (uint8_t)(g.my_node & 0xFF)) > 0)
            g.tx(rb, len);
    }

    meshtastic_Data d;
    if (!mt_data_decode(plain, pl, &d)) return;
    if (d.portnum == 0 && d.payload.size == 0) return;

    node_record_t *node = nodedb_upsert(&g.db, hdr.from, now);
    node->snr = snr; node->rssi = (int16_t)rssi;
    g.rx_count++; g.last_rssi = rssi; g.last_snr = snr;

    /* Delivery ack: a ROUTING reply carrying the request_id of one of our sends. */
    if (d.portnum == meshtastic_PortNum_ROUTING_APP && d.request_id != 0) {
        for (int i = 0; i < 16; i++) {
            if (g.ack_pending[i] == d.request_id) {
                uint32_t rid = d.request_id;
                if (g.bus) eventbus_publish(g.bus, EV_MESSAGE_ACK, &rid);
                g.ack_pending[i] = 0;
                break;
            }
        }
    }

    pktlog_add(hdr.from, (uint8_t)d.portnum, rssi, snr, now);
    if (s_rx_observer) s_rx_observer(hdr.from, hdr.to, hdr.id, plain, (int)pl, rssi, snr);

    net_message_t m = {0};
    m.from_id = hdr.from; m.snr = snr; m.rssi = (int)rssi; m.when = now;
    net_node_label(hdr.from, node->long_name, node->short_name, m.long_name, sizeof m.long_name);
    snprintf(m.short_name, sizeof m.short_name, "%s", node->short_name);

    switch (d.portnum) {
    case meshtastic_PortNum_TEXT_MESSAGE_APP: {
        m.kind = MSG_TEXT;
        int n = d.payload.size; if (n > MSG_TEXT_MAX) n = MSG_TEXT_MAX;
        memcpy(m.text, d.payload.bytes, (size_t)n); m.text[n] = 0;
        if (g.bus) eventbus_publish(g.bus, EV_MESSAGE_RECEIVED, &m);
        break;
    }
    case meshtastic_PortNum_POSITION_APP: {
        meshtastic_Position p;
        if (mt_position_decode(d.payload.bytes, d.payload.size, &p)) {
            node->lat_i = p.latitude_i; node->lon_i = p.longitude_i;
            node->alt = p.altitude; node->has_position = true;
            if (p.ground_speed || p.ground_track) {
                node->speed = (float)p.ground_speed;        /* m/s */
                node->course = (float)p.ground_track / 1e5f; /* degrees */
                node->has_motion = true;
            }
            m.kind = MSG_POSITION;
            m.lat = p.latitude_i / 1e7; m.lon = p.longitude_i / 1e7;
            m.alt = p.altitude; m.ts = p.time; m.sats = p.sats_in_view;
            if (g.bus) eventbus_publish(g.bus, EV_POSITION_UPDATE, &m);
        }
        break;
    }
    case meshtastic_PortNum_NODEINFO_APP: {
        meshtastic_User u;
        if (mt_user_decode(d.payload.bytes, d.payload.size, &u)) {
            snprintf(node->long_name, sizeof node->long_name, "%s", u.long_name);
            snprintf(node->short_name, sizeof node->short_name, "%s", u.short_name);
        }
        break;
    }
    case meshtastic_PortNum_TELEMETRY_APP: {
        meshtastic_Telemetry t;
        if (mt_telemetry_decode(d.payload.bytes, d.payload.size, &t)) {
            if (t.has_device_metrics) {
                node->battery = (uint8_t)t.device_metrics.battery_level;
                node->voltage = t.device_metrics.voltage;
                node->has_telemetry = true;
            }
            if (t.has_environment_metrics) {
                node->temperature = t.environment_metrics.temperature;
                node->humidity = t.environment_metrics.relative_humidity;
                node->pressure = t.environment_metrics.barometric_pressure;
                node->has_env = true;
            }
        }
        break;
    }
    default: break;
    }

    if (g.bus) eventbus_publish(g.bus, EV_MESH_NODE_UPDATE, node);
}

uint32_t mtb_my_node(void) { return g.my_node; }
const char *mtb_channel(void) { return g.channel; }
int mtb_peers(void) { return g.db.count; }
nodedb_t *mtb_nodedb(void) { return &g.db; }

void mtb_diag(net_diag_t *out)
{
    out->node = g.my_node;
    snprintf(out->region, sizeof out->region, "%s", g.region);
    snprintf(out->preset, sizeof out->preset, "%s", g.preset);
    snprintf(out->channel, sizeof out->channel, "%s", g.channel);
    out->freq_mhz = g.freq;
    out->sf = g.sf;
    out->sync_word = MESH_SYNC_WORD;
    out->hop_limit = g.hop_limit;
    out->relay = g.rebroadcast;
    out->peers = g.db.count;
    out->rx_count = g.rx_count;
    out->rx_raw = g.rx_raw;
    out->tx_count = g.tx_count;
    out->last_rssi = g.last_rssi;
    out->last_snr = g.last_snr;
}
