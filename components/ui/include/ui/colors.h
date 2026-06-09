/* Color tokens for the dark/amber theme (RGB hex, LVGL-free so they unit-test).
 * Ported from firmware/badge/ui/theme.py. The UI is monochrome-by-design: a
 * single amber accent (C_ACCENT) over a dark surface, with status colors only
 * for battery/signal semantics. */
#ifndef UI_COLORS_H
#define UI_COLORS_H

#define C_BG         0x0E1116  /* screen background */
#define C_SURFACE    0x171C24  /* card / row / bubble */
#define C_SURFACE_2  0x222A35  /* raised / selected row / input field */
#define C_SIDEBAR    0x0A0D11  /* sidebar base */
#define C_SIDEBAR_2  0x131922  /* sidebar gradient top */
#define C_DIVIDER    0x2A323D  /* hairlines / borders */

#define C_TEXT       0xF2F5F8  /* primary text */
#define C_TEXT_DIM   0x93A0AE  /* secondary (timestamps, hints) */
#define C_TEXT_MUTE  0x5A6573  /* disabled / placeholder */

#define C_ACCENT     0xE39810  /* primary accent (selection, my-bubble, icons) */
#define C_ACCENT_DK  0x8A5C06  /* accent pressed */
#define C_ON_ACCENT  0x1A1206  /* text on accent fills */

#define C_OK         0x35C46A  /* good */
#define C_WARN       0xF2B01E  /* warning */
#define C_CRIT       0xE5484D  /* critical */
#define C_IDLE       0x3A4350  /* inactive icon element */
#define C_CHARGE     0x3FC7E0  /* charging / USB cyan */

#define C_NONE       (-1L)     /* "no color" sentinel for mapper returns */

#endif /* UI_COLORS_H */
