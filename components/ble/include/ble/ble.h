/* Meshtastic BLE companion: pairs with the official phone app over Bluetooth so
 * the badge appears as a node it can read and send through.
 *
 * Off by default (the "ble_enabled" setting). This is a first-pass companion
 * implementation; the phone app is strict about the config handshake, so expect
 * to iterate on hardware. */
#ifndef BLE_BLE_H
#define BLE_BLE_H

#include "core/settings.h"

/* Register the ble_enabled setting, and start the BLE stack if it is on. */
void ble_init(settings_t *reg, const char *device_short_name);

#endif /* BLE_BLE_H */
