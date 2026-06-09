# Communicator Badge — C / ESP-IDF firmware

A from-scratch **C firmware** for the Hackaday 2024 Supercon **Communicator badge**
(ESP32-S3 + SX1262 LoRa + NV3007 TFT + TCA8418 keyboard). It replaces the stock
`lvgl_micropython` build with a native, event-driven ESP-IDF application focused on
three things:

- **Meshtastic messaging that works** — the badge is a real node on a Meshtastic mesh:
  it chats with other badges, Meshtastic devices, and the Meshtastic phone app over LoRa.
- **A clean, modern, monochrome UI** — one font everywhere, a persistent status sidebar,
  a full-width function-key bar, and a horizontally scrollable app launcher.
- **Battery efficiency** — interrupt-driven I/O and ESP32-S3 light sleep instead of the
  MicroPython busy-poll loops; the radio, keyboard, and timers wake the CPU on demand.

Repo: <https://github.com/giovi321/had-badge-mod>

## Why the rewrite

MicroPython on this device forced 1 ms busy-poll loops for the keyboard and radio, had
no real light-sleep path, reached LVGL through unverified bindings, and once let a CAD
tight-spin starve the whole event loop. Rewriting in C/ESP-IDF gives interrupt-driven
peripherals, true light sleep, and direct control of the SX1262 and the display.

## Hardware

| Part | Detail |
|------|--------|
| MCU | ESP32-S3-WROOM, 8 MB PSRAM (octal) / 16 MB flash |
| Display | NV3007 TFT, 428×142 (rotated 270°), SPI @80 MHz, RGB565 byte-swapped |
| Radio | Semtech SX1262 LoRa, TCXO on DIO3 @1.7 V, DIO2 + GPIO10 RF switch |
| Keyboard | TCA8418 I²C matrix controller (INT-driven) |
| GPS (optional) | ATGM336H NMEA over UART on header J6 (GPS-TX→IO12, ESP-TX=IO11) |
| Power | LiPo + MCP73831 charger; backlight PWM on GPIO2 |

All pins live in one place: [`components/bsp/include/board_pins.h`](components/bsp/include/board_pins.h).

## Architecture

ESP-IDF components, layered bottom-up. The `core`, `mesh`, `util`, and the portable parts
of `net`/`ui` carry **no ESP-IDF dependency** so the protocol/crypto/layout logic compiles
and is unit-tested on a PC.

```
main/                     app_main.c boot sequence, board_pins.h, app_config.h (bsp)
components/
  core/    PORTABLE  eventbus, schema settings (+ NVS store), node DB
  mesh/    PORTABLE  AES-128/256, Meshtastic crypto, packet, regions, nanopb protobufs
  util/    PORTABLE  NMEA parser
  net/     PORTABLE  message types, dedup, router + Meshtastic backend
  drivers/ ESP-IDF   NV3007 display, TCA8418 keyboard, SX1262 radio, GPS, battery, power
  radio/   ESP-IDF   radio task: RX-ISR→parse, TX queue, listen-before-talk
  services/ESP-IDF   status sidebar, mesh beacon, time-from-GPS, battery, gps
  ui/      LVGL      theme, frame, sidebar, full-width menubar, launcher strip, icons
  apps/    LVGL      launcher + Messages, Nodes, Settings, GPS
host_tests/          zero-dependency C test harness (no ESP-IDF)
tools/               run_host_tests.ps1, gen_proto / asset helpers
```

### Task / event model

| Task | Owns | Blocks on |
|------|------|-----------|
| `ui` | LVGL, apps, launcher, input dispatch | `lv_timer_handler()` interval |
| `radio` | SX1262 RX/TX/CAD state machine | DIO1 ISR semaphore (+ TX submits) |
| `kbd` | TCA8418 decode, F-key/modifier state | INT ISR semaphore |
| `mesh_svc` / `time` / `backlight` | beacons, clock, dimming | timers |

The synchronous **EventBus** carries `EV_MESSAGE_RECEIVED`, `EV_MESH_NODE_UPDATE`, etc.
LVGL is single-threaded: only the `ui` task touches it, so the radio task publishes into a
queue/snapshot that the UI drains on its own tick.

## Meshtastic

The on-air format is implemented directly and verified against real Meshtastic constants:

- **Protobufs:** official `meshtastic` messages (`Data`/`Position`/`User`) via **nanopb**
  (`components/mesh/proto/meshtastic.proto`, generated into `components/mesh/generated/`).
- **Crypto:** AES-CTR with the channel PSK; default channel hash `0x08`, nonce
  `pid·u32le | 0000 | from·u32le | 0000`, big-endian 128-bit counter.
- **Packet:** 16-byte little-endian header (to/from/id, flags, channel hash, hops); default
  LongFast flags `0x63`; **sync word `0x2B`**; dedup by `(from,id)`; rebroadcast **opt-in**
  (default off = handheld client).
- **Region/preset:** EU_868 LongFast → 869.525 MHz, US LongFast slot 19 → 906.875 MHz.

## Build & flash

Requires [ESP-IDF **5.1+**](https://docs.espressif.com/projects/esp-idf/). LVGL and nanopb
runtime are fetched/vendored automatically.

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

`sdkconfig.defaults` enables 16 MB flash, octal PSRAM, light sleep, and the LVGL NV3007
driver. The partition table is [`partitions.csv`](partitions.csv).

## Host tests (no hardware)

All portable logic is covered by a self-contained C test harness. With a C compiler you can
run it via CMake; the repo also ships a `zig cc` runner that needs no system toolchain:

```bash
python -m pip install ziglang
pwsh tools/run_host_tests.ps1        # 513 checks: AES KATs, channel hash, packet,
                                     # nanopb roundtrip, regions, NMEA, settings,
                                     # eventbus, node DB, dedup, UI layout geometry
```

These reproduce the Meshtastic wire constants byte-for-byte, so the interop core is proven
before anything is flashed.

## On-device bring-up checklist

The drivers are written against the ESP-IDF / LVGL APIs but can only be fully validated on
hardware. Verify in order:

1. **Display** — boot shows the UI; if mirrored/offset, adjust `lv_display_set_rotation` /
   `lv_nv3007_set_gap` in [`display_nv3007.c`](components/drivers/display_nv3007.c).
2. **Keyboard** — every key, modifier, and F1–F5 register (TCA8418 INT on GPIO13).
3. **Radio RX** — capture a real LongFast packet; confirm decode (validates sync `0x2B`,
   869.525 MHz, AES-CTR). If the SX1262 won't start, check the TCXO voltage (DIO3, 1.7 V).
4. **Messaging** — send a text badge↔phone and back.
5. **Light sleep** — current drops at idle; a keypress or incoming packet wakes instantly.

## UI

- **Full-width bottom bar** to the left screen edge, five evenly distributed F-key cells,
  text clipped with an ellipsis so it never runs off-screen.
- **Status sidebar** (battery / mesh / wifi / gps) that stops exactly where the bottom bar
  begins (`height = SCREEN_H − BOTTOMBAR_H`).
- **Launcher** is a horizontally scrollable strip of monochrome icon tiles; labels are a
  single ellipsised line, never wrapped.
- **One font** (Montserrat 14 body / 18 title) and one amber accent on a dark surface;
  icons are LVGL's built-in monochrome glyphs, recolored.

## Settings

Schema-driven, stored in NVS, editable in the Settings app: network stack, region, preset,
channel name + key, hop limit, relay (router) mode, position sharing, TX power, device/short
name, GPS enable + pins, backlight timeouts.

## Credits & license

MIT (see [`LICENSE.txt`](LICENSE.txt)). Built for the Hackaday Communicator badge. Uses
[LVGL](https://lvgl.io), [nanopb](https://jpa.kapsi.fi/nanopb/) (zlib), and the
[Meshtastic](https://meshtastic.org) protocol. The Meshtastic wire facts were validated
against the original MicroPython implementation this project replaces.
