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
bool net_send_position(double lat, double lon, int32_t alt, uint32_t ts) { return mtb_send_position(lat, lon, alt, ts); }
bool net_send_nodeinfo(void) { return mtb_send_nodeinfo(); }

uint32_t net_my_node(void) { return mtb_my_node(); }
const char *net_channel_name(void) { return mtb_channel(); }
int net_peer_count(void) { return mtb_peers(); }
nodedb_t *net_nodedb(void) { return mtb_nodedb(); }

void net_on_frame(const uint8_t *frame, int len, float rssi, float snr, uint32_t now)
{
    mtb_on_frame(frame, len, rssi, snr, now);
}
