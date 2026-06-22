---
title: Diagnostics
description: Live per-module status for the radio, GPS, WiFi, Bluetooth, and system.
---

The Diagnostics app shows live values that help confirm each subsystem is working and the badge
is configured the way you expect. The rows are grouped under a heading per module.

## LoRa mesh

- Node: this badge's node id.
- Radio: region, frequency, and spreading factor.
- Channel: the channel name and the sync word.
- RX/TX: decoded receive count and transmit count for this channel.
- Heard: every valid LoRa frame the radio demodulates on any channel, counted before the
  channel and decrypt filter.
- Signal: the last received signal strength and signal-to-noise ratio.
- Peers: the number of known nodes.

The RX/TX and Heard counters are the quickest way to tell what the radio is doing. If you send
a message and TX goes up, the firmware reached the radio. If a device is nearby but RX stays at
zero, look at Heard: if Heard is climbing the radio hears traffic and the problem is the
channel or key, and if Heard is also zero check the region and frequency against that device.

## GPS

- Fix: the GPS state — `off` when disabled in Settings, `no data` when the receiver is enabled
  but nothing is arriving over the UART (check wiring), `searching` when sentences are coming in
  but there is no lock yet, or `fix` with the used/in-view satellite counts and HDOP once locked.
- Pos: the current latitude and longitude.
- Data: NMEA sentences parsed and how long since the last byte. This is the quickest way to tell
  whether the module is talking at all — if it stays at `0 sent, none yet`, the receiver is wired
  wrong, unpowered, or disabled.

## WiFi

- State: the connection state, with signal strength when associated.
- IP: the assigned address when the link is up.

## Bluetooth

- State: off, advertising, or connected, for the Meshtastic companion link.

## System

- Battery: charge percentage and voltage, and whether USB power is present.
- Up/Heap: uptime and free heap.
