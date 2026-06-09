---
title: Bluetooth (Meshtastic app)
description: Pair the badge with the official Meshtastic phone app.
---

The badge can present a Meshtastic Bluetooth companion service, so the official Meshtastic
phone app pairs with it and treats it as a node it can read and send through.

Bluetooth is off by default. Enable it in Settings, under the Bluetooth group
(`ble_enabled`), then reboot. The badge advertises with its short name and the Meshtastic
service.

## What it does

When the app connects, the badge runs the Meshtastic config handshake: it sends its node
info, the nodes it has heard, the primary channel, and a config-complete marker. After that
it bridges packets both ways. Messages you send from the app go out over LoRa, and packets
the badge receives are passed up to the app.

## Status

This is a first-pass implementation of the companion protocol. The phone app is strict about
the handshake sequence, so pairing and full two-way operation may need adjustment against a
specific app version. If the app connects but does not finish loading, or messages do not
flow, that is the area to look at. Use the serial log to see the handshake and the bridged
packets.
