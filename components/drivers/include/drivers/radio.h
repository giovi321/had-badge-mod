/* SX1262 LoRa driver (low-level chip ops). Ported from the badge's RadioLib-
 * style MicroPython driver: TCXO on DIO3 @1.7V, DCDC regulator, DIO2 + GPIO10
 * RF switch, Meshtastic sync word 0x2B. The radio task (components/radio) drives
 * the RX/TX/CAD state machine on top of these. */
#ifndef DRIVERS_RADIO_H
#define DRIVERS_RADIO_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    double freq_mhz;
    double bw_khz;
    int sf;         /* 5..12 */
    int cr;         /* 5..8 (4/5..4/8) */
    int sync_word;  /* 0x2B for Meshtastic */
    int preamble;
    int power_dbm;
} radio_params_t;

esp_err_t radio_chip_init(const radio_params_t *p);
esp_err_t radio_chip_reconfigure(const radio_params_t *p);

/* Channel activity detection. Returns 1 = activity, 0 = free, -1 = error.
 * Blocks on DIO1 (bounded). */
int radio_chip_cad(void);

/* Transmit one frame; blocks until TX_DONE (or timeout). Restores RX after. */
esp_err_t radio_chip_transmit(const uint8_t *data, int len);

/* Put the receiver into continuous RX (RF switch -> RX). */
void radio_chip_start_rx(void);

/* Read a received frame after RX_DONE. Returns length, or -1 if none/CRC error.
 * Fills rssi/snr (dBm / dB) when non-NULL. */
int radio_chip_read_packet(uint8_t *buf, int cap, float *rssi, float *snr);

/* Block until a radio event (DIO1) or wake, up to timeout_ms. Returns true if
 * an event fired. radio_chip_wake() unblocks it early (used by TX submit). */
bool radio_chip_wait_event(int timeout_ms);
void radio_chip_wake(void);

void radio_chip_sleep(void);    /* warm sleep, config retained */
void radio_chip_standby(void);

#endif /* DRIVERS_RADIO_H */
