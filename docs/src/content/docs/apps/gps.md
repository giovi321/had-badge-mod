---
title: GPS
description: Read the current position from an attached GPS module.
---

The GPS app shows the current fix from an ATGM336H module wired to header J6. The module is
optional, and GPS is off until you enable it in Settings.

## Wiring

Connect the module's TX to ESP RX (IO12) and its RX to ESP TX (IO11), with VCC to 3V3 and
GND to GND. Then set `gps_enabled` in Settings. The RX and TX pins are configurable there if
your wiring differs.

## What it shows

When the receiver has a fix, the app shows latitude, longitude, altitude, and the satellite
count. Before a fix it shows "Searching".

The firmware parses RMC and GGA sentences. When the RMC sentence carries a valid time, the
system clock is set from it, which the badge uses for message timestamps and for naming track
files.
