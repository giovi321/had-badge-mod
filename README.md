# Communicator Badge firmware

A C / ESP-IDF firmware for the Hackaday 2024 Supercon Communicator badge
(ESP32-S3, NV3007 TFT, SX1262 LoRa, TCA8418 keyboard). It replaces the stock
`lvgl_micropython` build with a native, interrupt-driven application focused on
Meshtastic messaging, a clean monochrome UI, and lower power use.

Documentation: <https://giovi321.github.io/had-badge-mod>

## What it does

- Joins a Meshtastic mesh and exchanges text messages with badges, Meshtastic devices, and
  the Meshtastic phone app. The on-air format follows the real Meshtastic protocol.
- Presents a monochrome UI: one font, a single accent, a status sidebar, a full-width
  function-key bar, and a horizontally scrolling app launcher.
- Wakes the CPU on keyboard and radio interrupts instead of polling, and scales the CPU
  frequency to save power.

## Hardware

| Part | Detail |
|------|--------|
| MCU | ESP32-S3-WROOM, 8 MB PSRAM, 16 MB flash |
| Display | NV3007 TFT, 428x142, SPI |
| Radio | Semtech SX1262 LoRa |
| Keyboard | TCA8418 I2C matrix |
| GPS (optional) | ATGM336H over UART on header J6 |

Pins are in `components/bsp/include/board_pins.h`.

## Build and flash

Needs ESP-IDF 5.1 or newer (built on 6.2). From the repository root:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

See [Build and flash](https://giovi321.github.io/had-badge-mod/getting-started/building/) and
[First boot](https://giovi321.github.io/had-badge-mod/getting-started/bring-up/) for details.

## Host tests

The protocol, crypto, and layout logic runs on a PC with no hardware. With the bundled
`zig cc` compiler:

```bash
python -m pip install ziglang
pwsh tools/run_host_tests.ps1
```

A `host_tests/CMakeLists.txt` is also provided for gcc or clang with CMake. See
[Host tests](https://giovi321.github.io/had-badge-mod/development/host-tests/).

## Offline radar map

The Radar app can draw an offline OpenStreetMap vector map (roads + water) behind the node
blips. Build a map for your area on a PC, upload it over Wi-Fi, and toggle it with **F4**:

```bash
# Export your area as GeoJSON from https://overpass-turbo.eu, then:
python tools/osm2vmap.py --geojson area.geojson --out map.vmap
```

Open `http://<badge-ip>/map` to upload `map.vmap`, then press **F4** in Radar (with a GPS fix)
to show it — the map must cover where your fix is. See
[Offline maps](https://giovi321.github.io/had-badge-mod/development/maps/) for the full
walkthrough.

## Layout

```
main/         app_main.c, board_pins.h (bsp), app_config.h
components/
  core/  mesh/  util/  net/   portable logic (also runs in host_tests)
  drivers/  radio/  services/  esp-idf hardware and tasks
  ui/  apps/                   LVGL interface
host_tests/                    portable test suite
docs/                          Astro + Starlight documentation site
```

## Roadmap

Active development happens on the `dev` branch. Planned feature batch, in priority order:

- [ ] "Find my people" radar / PPI scope — plot nodes by range and bearing (code complete on `dev`, on-device check pending)
- [ ] Web tools: config backup/restore and OTA firmware update over the web UI (code complete on `dev`, on-device check pending)
- [ ] Multi-channel threaded UI and a "mesh IRC" group chat with nicks (code complete on `dev`, on-device check pending)
- [ ] Haptics: piezo buzzer and vibration motor for notifications
- [ ] IMU and magnetometer driver — a real compass (also upgrades Radar/Tracker/Follow heading)
- [ ] Waypoint/POI manager — save, name, navigate, and share points over the mesh
- [ ] BLE channel/contact provisioning for zero-typing onboarding
- [x] Offline vector map under the radar view (code complete on `dev`, on-device check pending)

## License

MIT, see [LICENSE.txt](LICENSE.txt). Uses [LVGL](https://lvgl.io),
[nanopb](https://jpa.kapsi.fi/nanopb/), and the [Meshtastic](https://meshtastic.org)
protocol.
