/* OTA firmware update: stream a new image to the inactive app slot and boot it,
 * with bootloader rollback as the safety net. Driven by the web UI. */
#ifndef SERVICES_OTA_H
#define SERVICES_OTA_H

#include <stddef.h>
#include "esp_err.h"

/* Confirm the running image so the bootloader keeps it (cancels the pending
 * rollback). A no-op unless the running image is in the pending-verify state.
 * Call once the system has booted far enough to be considered healthy. */
void ota_mark_valid(void);

/* Streaming write to the next OTA slot. total may be 0 if the size is unknown. */
esp_err_t ota_begin(size_t total);
esp_err_t ota_write(const void *data, size_t len);
esp_err_t ota_end(void);          /* validate image + set it as the boot slot */
void ota_abort(void);

/* Reboot shortly from now, so an in-flight HTTP response can flush first. */
void ota_reboot_soon(void);

#endif /* SERVICES_OTA_H */
