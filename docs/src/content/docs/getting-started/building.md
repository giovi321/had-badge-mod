---
title: Build and flash
description: Build the firmware and write it to the badge over USB.
---

With ESP-IDF active in your shell, build from the repository root:

```bash
idf.py set-target esp32s3
idf.py build
```

The first build fetches LVGL through the component manager and compiles ESP-IDF, so it
takes a few minutes. Later builds reuse the cache and are much faster. A clean build
produces an app of roughly 1.6 MB, about half of one of the two 3 MB application slots.

## Flash

Connect the badge over USB and find its serial port (for example `COM12` on Windows, or
`/dev/ttyACM0` on Linux). Then:

```bash
idf.py -p COM12 flash monitor
```

`flash` writes the bootloader, partition table, and application. `monitor` opens the serial
console so you can watch the boot log. Exit the monitor with `Ctrl+]`.

If the badge does not enter download mode on its own, hold BOOT, tap RESET, and release
BOOT, then run the command again.

## Flash a build you already have

`idf.py flash` reuses whatever is in `build/` if nothing changed, so it does not rebuild.
You can also flash with esptool directly from the `build` directory using the arguments the
build printed:

```bash
python -m esptool --chip esp32s3 -p COM12 write-flash "@flash_args"
```

## Notes on configuration

`sdkconfig.defaults` sets the target to ESP32-S3, 16 MB flash, octal PSRAM, and dynamic
frequency scaling. The LVGL NV3007 display driver is not exposed through LVGL's Kconfig, so
the top level `CMakeLists.txt` enables it (and its generic-MIPI dependency) with
`-DCONFIG_LV_USE_NV3007=1` and `-DCONFIG_LV_USE_GENERIC_MIPI=1`. You do not need to change
anything for a normal build.

## Partitions and over-the-air updates

`partitions.csv` carries two 3 MB application slots (`ota_0` and `ota_1`) plus the `otadata`
marker, so a new image can be written to the spare slot — over USB or through the web UI — and
booted with rollback if it fails to start. The rest of the flash is an ~9.6 MB SPIFFS area for
GPS tracks and assets.

Changing the partition table is a one-time USB flash: a new table, and the SPIFFS area that
moves with it, cannot be delivered over the air, and the SPIFFS reformats (saved GPS tracks
are lost). After that first cable flash you can update over the air from
[the web UI](/had-badge-mod/connectivity/wifi/).
