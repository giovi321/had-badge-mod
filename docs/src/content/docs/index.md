---
title: Communicator Badge firmware
description: A C / ESP-IDF Meshtastic firmware for the Hackaday 2024 Supercon Communicator badge.
template: splash
hero:
  tagline: A native C firmware for the Communicator badge. Meshtastic messaging, a clean monochrome UI, and interrupt-driven power use.
  actions:
    - text: Get started
      link: /had-badge-mod/getting-started/installation/
      icon: right-arrow
    - text: GitHub
      link: https://github.com/giovi321/had-badge-mod
      icon: external
      variant: minimal
---

This firmware replaces the stock `lvgl_micropython` build on the Hackaday 2024 Supercon
Communicator badge (ESP32-S3, NV3007 TFT, SX1262 LoRa, TCA8418 keyboard). It is written
in C against ESP-IDF.

## What it does

The badge joins a Meshtastic mesh and exchanges text messages with other badges, Meshtastic
devices, and the Meshtastic phone app over LoRa. The on-air format follows the real
Meshtastic wire protocol, so packets interoperate with stock devices.

Messages can run on several channels at once and track delivery with retries and optional read
receipts. A radar view plots nearby nodes by range and bearing, and when WiFi is on a web UI
configures the badge, backs up its settings, and flashes firmware over the air.

The interface uses one font, a single amber accent on a dark background, and monochrome
icons. A status sidebar runs down the left edge, a function-key bar spans the bottom, and
the app launcher is a horizontally scrolling strip of icon tiles.

Peripherals are interrupt driven. The keyboard and radio wake the CPU on demand instead of
the busy-poll loops the MicroPython build needed, and the CPU scales its frequency to save
power.

## Where to go next

Install the toolchain on the [Install ESP-IDF](/had-badge-mod/getting-started/installation/)
page, then [build and flash](/had-badge-mod/getting-started/building/) the firmware. If you
want to understand how it fits together, read the
[architecture overview](/had-badge-mod/architecture/overview/).

The protocol and layout logic is covered by a host test suite that runs on a PC with no
hardware. See [Host tests](/had-badge-mod/development/host-tests/).
