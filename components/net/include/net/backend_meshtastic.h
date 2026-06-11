/* Meshtastic backend internals, driven by the router facade (router.c). */
#ifndef NET_BACKEND_MESHTASTIC_H
#define NET_BACKEND_MESHTASTIC_H

#include "net/backend.h"

void mtb_register_settings(settings_t *st);
void mtb_init(settings_t *st, eventbus_t *bus, uint32_t my_node);
void mtb_reload(void);
void mtb_radio_cfg(net_radio_cfg_t *out);
void mtb_set_tx(net_tx_fn_t fn);

bool mtb_send_text(const char *text);
bool mtb_send_text_to(uint32_t to_id, const char *text);
bool mtb_send_position(double lat, double lon, int32_t alt, uint32_t ts);
bool mtb_send_nodeinfo(void);
bool mtb_send_telemetry(int battery_pct, float voltage, uint32_t uptime_s);
int mtb_packet_log(net_pkt_log_t *out, int max);
void mtb_set_rx_observer(net_rx_observer_fn fn);
bool mtb_send_meshpacket(uint32_t to, bool want_ack, const uint8_t *data, int data_len);
void mtb_on_frame(const uint8_t *frame, int len, float rssi, float snr, uint32_t now);
void mtb_tick(uint32_t now);

uint32_t mtb_my_node(void);
const char *mtb_channel(void);
int mtb_peers(void);
nodedb_t *mtb_nodedb(void);
void mtb_diag(net_diag_t *out);

#endif /* NET_BACKEND_MESHTASTIC_H */
