/* Persistent left status sidebar on lv_layer_top(). Height = SCREEN_H -
 * BOTTOMBAR_H so it stops exactly where the full-width bottom bar begins. */
#ifndef UI_SIDEBAR_H
#define UI_SIDEBAR_H

#include "ui/status.h"

void sidebar_init(void);
void sidebar_update(const status_snapshot_t *s);

#endif /* UI_SIDEBAR_H */
