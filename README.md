<h1 align="center">Communicator Badge firmware</h1>

<p align="center">
  Native C / ESP-IDF Meshtastic firmware for the Hackaday 2024 Supercon Communicator badge.
</p>

<p align="center">
  <a href="https://github.com/giovi321/had-badge-mod/actions/workflows/docs.yml"><img src="https://github.com/giovi321/had-badge-mod/actions/workflows/docs.yml/badge.svg" alt="Docs"></a>
  <a href="https://github.com/giovi321/had-badge-mod/releases/latest"><img src="https://img.shields.io/github/v/release/giovi321/had-badge-mod" alt="Latest release"></a>
  <a href="LICENSE.txt"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License: MIT"></a>
  <img src="https://img.shields.io/badge/platform-ESP32--S3-informational" alt="Platform: ESP32-S3">
  <img src="https://img.shields.io/badge/ESP--IDF-5.1%2B-E7352C?logo=espressif&logoColor=white" alt="ESP-IDF 5.1+">
  <img src="https://img.shields.io/badge/Meshtastic-protocol-67EA94" alt="Meshtastic protocol">
</p>

<p align="center">
  <a href="https://giovi321.github.io/had-badge-mod/"><img src="https://img.shields.io/badge/Read_the_docs-2563EB?style=for-the-badge&logo=readthedocs&logoColor=white" alt="Read the documentation"></a>
</p>

> Replaces the stock `lvgl_micropython` demo with a native firmware. Flashing is reversible: you can put the original image back over USB whenever you want.

The Communicator badge from Supercon 2024 is a real LoRa radio hiding under a demo app. This firmware turns it into a Meshtastic messenger you would actually carry. It joins a mesh, exchanges encrypted text with other badges, Meshtastic devices, and the phone app, and runs a clean monochrome UI that wakes on interrupts instead of polling.

The protocol, crypto, and layout logic are portable C that also build and run on a PC, so most of the firmware is testable without touching hardware.

<p align="center">
  <img src="docs/src/assets/architecture-tasks.svg" alt="Producer tasks publish to an event bus that feeds the single UI task driving the display" width="820" />
</p>

## Quick start

With ESP-IDF 5.1 or newer active in your shell (built on 6.2), from the repository root:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

First boot blank or white? That is almost always stale NVS left by the previous firmware. Run `idf.py -p <PORT> erase-flash` once, then flash again. See [Build and flash](https://giovi321.github.io/had-badge-mod/getting-started/building/) and [First boot](https://giovi321.github.io/had-badge-mod/getting-started/bring-up/) for the rest.

## What it does

- **Meshtastic messaging.** Joins a mesh and exchanges text with badges, Meshtastic nodes, and the phone app. The on-air format follows the real Meshtastic protocol, so the badge is a first-class node, not a bridge.
- **Monochrome UI.** One font, a single accent, a status sidebar, a full-width function-key bar, and a horizontally scrolling app launcher.
- **Low power by design.** Wakes the CPU on keyboard and radio interrupts instead of polling, and scales the clock down when idle.
- **Offline maps under radar.** Draws an OpenStreetMap vector map (roads and water) behind the node blips, built on a PC and uploaded over Wi-Fi.
- **GPS, when you want it.** Wire an ATGM336H to the J6 header for position, course, and a clock the badge sets itself. A 3D-printable case with a bay for the module lives in [`hardware/case/`](hardware/case/).

## Offline radar map

The Radar app can draw an offline OpenStreetMap vector map (roads and water) behind the node blips. Build a map for your area on a PC, upload it over Wi-Fi, and toggle it with **F4**:

```bash
# Export your area as GeoJSON from https://overpass-turbo.eu, then:
python tools/osm2vmap.py --geojson area.geojson --out map.vmap
```

Open `http://<badge-ip>/map` to upload `map.vmap`, then press **F4** in Radar (with a GPS fix) to show it. The map has to cover where your fix is. See [Offline maps](https://giovi321.github.io/had-badge-mod/development/maps/) for the full walkthrough.

## Hardware

| Part | Detail |
|------|--------|
| MCU | ESP32-S3-WROOM, 8 MB PSRAM, 16 MB flash |
| Display | NV3007 TFT, 428x142, SPI |
| Radio | Semtech SX1262 LoRa |
| Keyboard | TCA8418 I2C matrix |
| GPS (optional) | ATGM336H over UART on header J6 |
| Case (optional) | 3D-printable, with a GPS bay — [`hardware/case/`](hardware/case/) |

Pins live in `components/bsp/include/board_pins.h`.

## Host tests

The portable logic (protocol, crypto, layout) runs on a PC with no hardware. With the bundled `zig cc` compiler:

```bash
python -m pip install ziglang
pwsh tools/run_host_tests.ps1
```

A `host_tests/CMakeLists.txt` is also provided for gcc or clang with CMake. See [Host tests](https://giovi321.github.io/had-badge-mod/development/host-tests/).

## Project layout

```
main/         app_main.c, board_pins.h (bsp), app_config.h
components/
  core/  mesh/  util/  net/    portable logic (also runs in host_tests)
  drivers/  radio/  services/   esp-idf hardware and tasks
  ui/  apps/                    LVGL interface
hardware/case/                  3D-printable case (STL + notes)
host_tests/                     portable test suite
docs/                           Astro + Starlight documentation site
```

## Roadmap

The latest release, **v0.12.0**, shipped the offline vector map under radar, GPS diagnostics, the printable GPS case, and the fix for the WiFi-on white screen.

Planned next, in rough priority order:

- [ ] "Find my people" radar / PPI scope — plot nodes by range and bearing (code complete, on-device check pending)
- [ ] Web tools: config backup/restore and OTA firmware update over the web UI (code complete, on-device check pending)
- [ ] Multi-channel threaded UI and a "mesh IRC" group chat with nicks (code complete, on-device check pending)
- [ ] Haptics: piezo buzzer and vibration motor for notifications
- [ ] IMU and magnetometer driver — a real compass (also upgrades Radar, Tracker, and Follow heading)
- [ ] Waypoint/POI manager — save, name, navigate, and share points over the mesh
- [ ] BLE channel/contact provisioning for zero-typing onboarding

## License

MIT, see [LICENSE.txt](LICENSE.txt). Uses [LVGL](https://lvgl.io), [nanopb](https://jpa.kapsi.fi/nanopb/), and the [Meshtastic](https://meshtastic.org) protocol.
