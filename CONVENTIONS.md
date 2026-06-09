# Conventions

## Layering

- **Portable first.** Protocol, crypto, packet, region, NMEA, settings, node DB, dedup, and
  UI layout/geometry are written as pure C11 with **no ESP-IDF / LVGL / FreeRTOS includes**,
  in `components/{core,mesh,util}` and the `portable/` subdirs of `net`/`ui`. They compile
  into the firmware *and* into `host_tests/`. If logic can be tested on a PC, it lives here.
- **Drivers own hardware.** `components/drivers` wraps esp_lcd / I²C / SPI / UART / LEDC /
  ADC / PM. Everything else talks to hardware only through these.
- **One source of truth for pins:** `components/bsp/include/board_pins.h`.

## Threads & LVGL

- **Only the `ui` task calls LVGL.** Other tasks publish on the EventBus or push to a queue;
  the owning app drains it in its `tick()` (which runs in the UI task). Never touch an
  `lv_obj_*` from the radio, keyboard, or service tasks.
- EventBus handlers run on the publisher's stack: keep them short, non-blocking, no LVGL.
- ISRs only give a semaphore / queue from `IRAM_ATTR` handlers; decoding happens in a task.

## UI

- One font everywhere via `theme_font_body()` / `theme_font_title()`; apps must not set
  fonts directly. One amber accent (`C_ACCENT`) on a dark surface; status colors only carry
  battery/signal semantics. Icons are LVGL's built-in monochrome glyphs, recolored.
- All on-screen text must stay readable on every background it can appear on — use the
  theme color tokens, never hard-coded colors.

## Style

- C11, 4-space indent, K&R braces. Match the surrounding file.
- Static/file-local by default; expose the minimum through `include/<component>/...`.
- Fixed-capacity buffers, no `malloc` in steady state (the radio/UI paths must not allocate).
- Functions return `esp_err_t` or a small int status; log with the component `TAG`.
- Comments explain *why* and cite the ported source where relevant, not the obvious.

## Meshtastic interop is load-bearing

The wire constants (channel hash `0x08`, flags `0x63`, nonce layout, sync word `0x2B`,
region frequencies) are frozen as host-test KATs in `host_tests/`. Change the codec only
with a passing test that pins the new bytes — a wrong byte silently breaks interop.

## Settings

- NVS keys are **≤ 15 characters** (hardware limit). Labels can be long; keys can't.
- Everything is stored as a string; typed access goes through `settings_get_*/set_*`.

## Tests

- Run `pwsh tools/run_host_tests.ps1` (or the CMake target) before every commit; it must be
  green. It's the only pre-flash safety net.
- Device behavior is validated with the bring-up checklist in the README.

## Git

- Author commits as Giovanni; no AI/assistant trailers.
- Keep the `upstream/` reference clone gitignored — it is not part of this repo.
