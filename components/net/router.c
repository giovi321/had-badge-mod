/* Network router facade (net_*). v1 forwards everything to the Meshtastic
 * backend; the indirection keeps room for a second backend later. */
#include "net/backend.h"
#include "net/backend_meshtastic.h"

void net_register_settings(settings_t *settings) { mtb_register_settings(settings); }
void net_init(settings_t *settings, eventbus_t *bus, uint32_t my_node) { mtb_init(settings, bus, my_node); }
void net_set_tx(net_tx_fn_t fn) { mtb_set_tx(fn); }
void net_reload_config(void) { mtb_reload(); }
void net_radio_cfg(net_radio_cfg_t *out) { mtb_radio_cfg(out); }

bool net_send_text(const char *text) { return mtb_send_text(text); }
bool net_send_text_to(uint32_t to_id, const char *text) { return mtb_send_text_to(to_id, text); }
bool net_send_position(double lat, double lon, int32_t alt, uint32_t ts) { return mtb_send_position(lat, lon, alt, ts); }
bool net_send_nodeinfo(void) { return mtb_send_nodeinfo(); }
bool net_send_telemetry(int battery_pct, float voltage, uint32_t uptime_s) { return mtb_send_telemetry(battery_pct, voltage, uptime_s); }
int net_packet_log(net_pkt_log_t *out, int max) { return mtb_packet_log(out, max); }
void net_set_rx_observer(net_rx_observer_fn fn) { mtb_set_rx_observer(fn); }
bool net_send_meshpacket(uint32_t to, bool want_ack, const uint8_t *data, int data_len) { return mtb_send_meshpacket(to, want_ack, data, data_len); }

uint32_t net_my_node(void) { return mtb_my_node(); }
const char *net_channel_name(void) { return mtb_channel(); }
int net_peer_count(void) { return mtb_peers(); }
nodedb_t *net_nodedb(void) { return mtb_nodedb(); }

void net_on_frame(const uint8_t *frame, int len, float rssi, float snr, uint32_t now)
{
    mtb_on_frame(frame, len, rssi, snr, now);
}

void net_diag(net_diag_t *out) { mtb_diag(out); }
