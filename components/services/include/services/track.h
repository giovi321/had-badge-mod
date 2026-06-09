/* GPS breadcrumb recording to timestamped files on the SPIFFS storage partition. */
#ifndef SERVICES_TRACK_H
#define SERVICES_TRACK_H

#include <stdbool.h>
#include "esp_err.h"

#define TRACK_DIR "/spiffs"

/* Mount the storage partition and start the recording task. */
esp_err_t track_svc_init(void);

/* Start a new track. Returns false if the clock is not set yet (no GPS time),
 * since the file is named from the start time. */
bool track_start(void);
void track_stop(void);

bool track_is_active(void);
int  track_point_count(void);
const char *track_filename(void);   /* basename of the current/last track */

#endif /* SERVICES_TRACK_H */
