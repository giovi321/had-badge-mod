/* Minimal web UI: a settings form and a diagnostics page over HTTP. */
#ifndef SERVICES_WEB_H
#define SERVICES_WEB_H

#include "core/settings.h"
#include "esp_err.h"

esp_err_t web_svc_start(settings_t *reg);
void web_svc_stop(void);

#endif /* SERVICES_WEB_H */
