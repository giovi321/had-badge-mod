/* Radio task: owns the SX1262 RX/TX/listen-before-talk loop and bridges
 * received frames into the Meshtastic backend. */
#ifndef RADIO_TASK_H
#define RADIO_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* Initialise the radio from the net config and start the radio task. */
esp_err_t radio_task_start(void);

/* Submit a frame for transmission (thread-safe). Registered as the net TX fn. */
bool radio_tx_submit(const uint8_t *frame, int len);

#endif /* RADIO_TASK_H */
