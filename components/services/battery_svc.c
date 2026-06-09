/* Battery service: brings up the ADC sense (off by default; no confirmed
 * divider on the stock board). */
#include "services/services.h"
#include "drivers/battery.h"
#include "board_pins.h"

void battery_svc_init(void)
{
    battery_init(PIN_BAT_ADC, 2, 1);   /* assumes a 2:1 divider when a pin is set */
}
