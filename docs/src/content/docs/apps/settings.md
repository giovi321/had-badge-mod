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
  position interval, how often this node announces itself, and transmit power.
- Device: device name and short name.
- Messages: how many messages to keep across boots.
- GPS: enable the receiver and set its UART pins.
- Bluetooth: enable the Meshtastic companion link.
- Power: backlight dim timeout, off timeout, and the bright and dim levels.

## Editing

Move with the arrow keys and open a setting to edit it. Booleans toggle, enumerations cycle
through their choices, and text fields open an editor you can type into. Esc backs out one
level.

A number opens a dedicated editor. It shows the current value large at the top, a slider that
tracks where the value sits in its allowed range, and the range itself as "Range x to y". Use
the arrow keys to step the value, or type a new one straight into the field below. Out-of-range
entries are not accepted. F1 saves.

## Announce interval

Under Radio, "Announce me every (s)" sets how often the badge tells the other nodes it exists,
by broadcasting its node info and telemetry. The default is 300 seconds. You can also send one
straight away with the Announce function key in the Messages and Nodes apps, without changing
this interval.

## Radio changes

Radio settings such as region, preset, channel, and key take effect on the next radio
reconfigure or reboot. If you change the channel and want to talk to a specific device, match
its region, channel name, and key exactly.
