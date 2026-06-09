---
title: First boot
description: What to check the first time the firmware runs on a badge.
---

The drivers are written against the ESP-IDF and LVGL APIs, but only hardware can confirm
the wiring. After the first flash, watch the serial monitor and check these in order.

## Boot log

A healthy boot prints lines like:

```
main: Communicator-C 1.0.0  node !aabbccdd
display: NV3007 display up (428x142, rot270, gap 0/12)
kbd: TCA8418 keyboard up
sx1262: SX1262 up: 869.525 MHz SF11 BW250 CR4/5 sync 0x2B pwr 9dBm
radio_task: radio task up
apps: app manager ready (5 apps)
power: DFS enabled (80-240 MHz)
main: boot complete
```

The node id comes from the eFuse MAC, so it is stable across boots.

## Display

The launcher should appear right side up: a "Communicator" title, a row of app tiles, the
status sidebar on the left, and the function-key bar across the bottom. If the image is
upside down or mirrored, adjust the rotation in
`components/drivers/display_nv3007.c` (`lv_display_set_rotation`) and the column gap
(`lv_nv3007_set_gap`).

## Keyboard

Arrow keys move the launcher selection, Enter or F3 opens an app, and Esc or F5 returns to
the launcher. Every key, modifier, and function key should register once per press.

## Radio

Confirm the radio receives a real packet. With another Meshtastic device on the same region
and channel, the Nodes app should list it within a minute, and the Diagnostics app shows the
receive counter increasing. This validates the sync word (`0x2B`), the frequency, and the
channel decryption together.

Then send a text message from the badge to the Meshtastic phone app and back.

## If the screen stays dark

Read the boot log. If `display: NV3007 display up` never prints, the display init did not
finish. If the log stops right after `Communicator-C`, something earlier in
`app_main()` is blocking. The [troubleshooting page](/had-badge-mod/development/troubleshooting/)
covers the failures seen so far.
