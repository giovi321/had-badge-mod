/* Network router facade. v1 is Meshtastic-only, but the seam stays thin so a
 * second backend could be added. The radio task calls net_on_frame() on RX and
 * registers its TX submit via net_set_tx(). */
#ifndef NET_BACKEND_H
#define NET_BACKEND_H

#include <stdint.h>
#include <stdbool.h>
#include "core/settings.h"
#include "core/eventbus.h"
#include "core/nodedb.h"
#include "net/message.h"

/* Resolved LoRa modem parameters (kept driver-agnostic so net needn't depend on
 * the radio driver). */
typedef struct {
    double freq_mhz, bw_khz;
    int sf, cr, sync_word, preamble, power_dbm;
} net_radio_cfg_t;

typedef bool (*net_tx_fn_t)(const uint8_t *frame, int len);

/* Snapshot of mesh diagnostics for the Diagnostics app / WebUI. */
typedef struct {
    uint32_t node;
    char region[16];
    char preset[16];
    char channel[24];
    double freq_mhz;
    int sf;
    int sync_word;
    int hop_limit;
    bool relay;
    int peers;
    uint32_t rx_count;
    uint32_t tx_count;
    float last_rssi;
    float last_snr;
} net_diag_t;

void net_diag(net_diag_t *out);

/* Recent received packets (for the packet-log app). */
typedef struct {
    uint32_t from;
    uint8_t portnum;
    int16_t rssi;
    float snr;
    uint32_t when;
} net_pkt_log_t;
int net_packet_log(net_pkt_log_t *out, int max);   /* most recent first; returns count */

void net_init(settings_t *settings, eventbus_t *bus, uint32_t my_node);
void net_register_settings(settings_t *settings);  /* schema only (call early) */
void net_set_tx(net_tx_fn_t fn);
void net_reload_config(void);
void net_radio_cfg(net_radio_cfg_t *out);

bool net_send_text(const char *text);                       /* broadcast */
bool net_send_text_to(uint32_t to_id, const char *text);    /* unicast (want_ack) */
bool net_send_position(double lat, double lon, int32_t alt, uint32_t ts);
bool net_send_nodeinfo(void);
bool net_send_telemetry(int battery_pct, float voltage, uint32_t uptime_s);

uint32_t net_my_node(void);
const char *net_channel_name(void);
int net_peer_count(void);
nodedb_t *net_nodedb(void);

/* RX entry point (radio task). now = unix seconds. */
void net_on_frame(const uint8_t *frame, int len, float rssi, float snr, uint32_t now);

#endif /* NET_BACKEND_H */
