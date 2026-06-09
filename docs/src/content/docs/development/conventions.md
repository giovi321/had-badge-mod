---
title: Conventions
description: How the code is organised and the rules to keep it consistent.
---

## Layering

Portable logic comes first. Protocol, crypto, packet, region, NMEA, settings, node DB, dedup,
and UI layout are plain C11 with no ESP-IDF, LVGL, or FreeRTOS includes. They live in
`components/core`, `components/mesh`, `components/util`, and the `portable/` subdirectories of
`net` and `ui`. If logic can be tested on a PC, it belongs there.

Drivers own the hardware. `components/drivers` wraps the LCD, I2C, SPI, UART, PWM, ADC, and
power management. Everything else reaches hardware only through those drivers.

There is one source of truth for pins, `components/bsp/include/board_pins.h`.

## Threads and LVGL

Only the `ui` task calls LVGL. Other tasks publish on the event bus or push to a queue, and
the owning app drains it on its own timer inside the UI task. Never touch an `lv_obj_*` from
the radio, keyboard, or service tasks.

Event bus handlers run on the publisher's stack, so they stay short and do not block.
Interrupt handlers only give a semaphore or queue from IRAM; the decoding happens in a task.

## Style

C11, four-space indent, K&R braces, matching the surrounding file. Default to file-local
static and expose the minimum through `include/<component>/`. Use fixed-capacity buffers and
avoid malloc in the steady-state radio and UI paths. Comments explain why, and cite the
source they were ported from where that helps.

## Settings

NVS keys are limited to 15 characters, so keep keys short. Labels can be long. Everything is
stored as a string, and typed access goes through `settings_get_*` and `settings_set_*`.

## Tests

Run the host tests before every commit. They are the only check that runs without hardware.
On-device behavior is validated with the bring-up checklist.
