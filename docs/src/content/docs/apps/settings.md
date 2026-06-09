---
title: Settings
description: Configure the radio, device, and display from the badge.
---

Settings is schema driven. Every option is declared once in the firmware with a type, a
default, a label, and a group, and the Settings app renders that schema. The same schema is
the basis for the web interface, so the two stay in sync.

Values are stored in NVS as strings and read back with typed accessors, so they survive
reboots.

## Groups

Settings are organised into groups so related options stay together:

- Network: the active network stack.
- Radio: region, preset, channel name, channel key, hop limit, relay mode, position sharing,
  position interval, and transmit power.
- Device: device name and short name.
- Messages: how many messages to keep across boots.
- GPS: enable the receiver and set its UART pins.
- Power: backlight dim timeout, off timeout, and the bright and dim levels.

## Editing

Move with the arrow keys and open a setting to edit it. Booleans toggle, enumerations cycle
through their choices, numbers step up and down, and text fields open an editor you can type
into. Esc backs out one level.

## Radio changes

Radio settings such as region, preset, channel, and key take effect on the next radio
reconfigure or reboot. If you change the channel and want to talk to a specific device, match
its region, channel name, and key exactly.
