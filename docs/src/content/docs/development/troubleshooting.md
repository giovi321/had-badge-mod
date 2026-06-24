---
title: Troubleshooting
description: Build and bring-up problems seen so far, and how to fix them.
---

## Build

### LVGL fails an integrity check

If the first build stops with a message about a missing `.component_hash` or `CHECKSUMS.json`
for `lvgl/lvgl`, the managed component download was interrupted and left incomplete. Delete
the partial state and let the component manager fetch it again:

```bash
rm -rf managed_components dependencies.lock build
idf.py set-target esp32s3
idf.py build
```

### lv_nv3007_create is undefined

LVGL's component does not expose the NV3007 driver through Kconfig, so a `CONFIG_LV_USE_NV3007`
line in `sdkconfig.defaults` is ignored. The top level `CMakeLists.txt` enables the driver for
the whole build with `-DCONFIG_LV_USE_NV3007=1`. If you removed that line, add it back.

### A header from another component is not found

If a build error says a component is not in the requirements list, add it to `REQUIRES` in
that component's `CMakeLists.txt`. The error message names both the component and the missing
dependency.

### A console encoding warning on Python 3.14

Set `PYTHONUTF8=1` in the shell before running `idf.py`.

## Boot

### The board reboots with a task watchdog on IDLE0

A task is busy-waiting and starving a core. The case seen during bring-up was the display
init: `lv_nv3007_create` runs the panel init and calls `lv_delay_ms`, which busy-waits on the
LVGL tick. The fix is to start the LVGL tick and register a yielding delay callback before
creating the display, which the firmware now does.

### Guru Meditation, interrupt watchdog, with a GPIO ISR in the backtrace

A GPIO interrupt is storming. The cause seen during bring-up was light-sleep wakeup:
`gpio_wakeup_enable` with a level trigger changes a pin's interrupt type to level triggered,
which collides with the edge ISR on the same pin. The radio DIO1 line then storms once it
goes high. The firmware keeps dynamic frequency scaling but does not enable level-triggered
GPIO wakeup, so light sleep is a later refinement.

### invalid panel io handle during display init

The NV3007 driver runs its init sequence inside `lv_nv3007_create`, before any display user
data is set. The send callbacks must use the panel IO handle from a file-local variable that
is set before the create call, not from LVGL user data.

### The screen is solid white, but the board boots and the radio works

The boot log is healthy — `display: NV3007 display up` prints, the keyboard and radio come up,
the node announces — yet the panel stays a uniform lit white. If swapping the panel with a
known-good badge moves nothing (the fault stays with the mainboard), the cause is usually
**stale `nvs` left by the badge's previous firmware**, not hardware.

A plain `idf.py flash` never erases the `nvs` partition, so settings written by the old
firmware survive. They can be read back as out-of-spec values (the settings read path does not
clamp integers to their schema range, only the write path does) and break the running firmware
while the display init itself still reports success. A full chip erase clears it:

```bash
idf.py -p COM6 erase-flash
idf.py -p COM6 flash monitor
```

Do this once when first flashing over the original firmware (see
[Build and flash](/had-badge-mod/getting-started/building/)). If a confirmed-good panel is
still white *after* a full erase and reflash, then suspect the hardware: reseat the display
FPC and continuity-check the control lines RST (GPIO40), CS (41), DC (39), SCLK (38), MOSI (21).

### The image is upside down or mirrored

Adjust the rotation in `components/drivers/display_nv3007.c`
(`lv_display_set_rotation`). If it comes out mirrored instead of rotated, change the panel
mirror flags. The column offset is set with `lv_nv3007_set_gap`.

## Radio

### No packets received

Open the Diagnostics app and check the region, preset, channel name, and sync word against
the device you expect to hear. The receive counter should climb when a packet is decoded. If
the radio never starts, check the TCXO voltage on DIO3 in the SX1262 driver.
