---
title: GPS
description: Read the current position from an attached GPS module.
---

The GPS app shows the current fix from an ATGM336H module wired to header J6. The module is
optional, and GPS is off until you enable it in Settings.

## Wiring

The GPS sits on the 4-pin J6 "Expansion" header. It is not populated from the factory, so you
solder your own header or wires. The pads are:

| J6 pin | Signal | ESP32-S3 |
|--------|--------|----------|
| 1 (square pad) | 3V3 | — |
| 2 | ESP TX → module RX | GPIO11 |
| 3 | ESP RX ← module TX | GPIO12 |
| 4 | GND | — |

So connect the module's **TX to J6 pin 3 (IO12)**, its **RX to J6 pin 2 (IO11)**, VCC to pin 1
(3V3), and GND to pin 4. The IO12 silkscreen is easy to misread as "IO10" — J6's signal pins are
IO11 and IO12; GPIO10 is the LoRa antenna switch and is not on this header.

Then set `gps_enabled` in Settings and reboot (the receiver starts at boot).

The RX and TX pins are configurable in Settings, so you do not have to match the default wiring.
If the GPS page shows **No data**, the TX and RX are almost certainly swapped — either swap the
two data wires, or, without touching the wiring, swap the pin numbers in Settings (set **GPS RX
pin** to the pad your module's TX is on and **GPS TX pin** to the module's RX pad) and reboot.

## What it shows

The Status row tells you where the receiver is at:

- **Disabled** — GPS is off in Settings; nothing is running.
- **No data** — GPS is enabled but no NMEA is arriving over the UART. Check the J6 wiring,
  power, and the RX/TX pins.
- **Searching** — sentences are coming in but there is no satellite lock yet.
- **Fix** — locked; the position rows are live.

The other rows show satellites used / in view, fix quality and HDOP (accuracy), latitude,
longitude, altitude, speed, course, the GPS time, and a Data row with how many NMEA sentences
have been parsed and how long since the last byte. The Data row is the fastest way to confirm
the module is talking at all.

The firmware parses RMC, GGA, and GSV sentences. When the RMC sentence carries a valid time, the
system clock is set from it, which the badge uses for message timestamps and for naming track
files.
