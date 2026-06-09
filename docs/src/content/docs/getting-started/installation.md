---
title: Install ESP-IDF
description: Set up the ESP-IDF toolchain needed to build the firmware.
---

The firmware builds with Espressif's ESP-IDF. Version 5.1 or newer works, and the build is
verified on 6.2. The IDF component manager fetches LVGL and the rest of the managed
dependencies on the first build, so you need network access the first time.

## Windows

Use the official ESP-IDF Windows installer from
[dl.espressif.com](https://dl.espressif.com/dl/esp-idf/). It installs ESP-IDF, Python,
CMake, Ninja, and the toolchains, and adds two Start menu shortcuts:

- "ESP-IDF PowerShell"
- "ESP-IDF CMD"

Open one of those. It runs the export script for you, so `idf.py` is on the path.

If you installed ESP-IDF manually, run the export script in each new terminal:

```powershell
C:\Users\<you>\esp-idf\export.ps1
```

On Python 3.14, set `PYTHONUTF8=1` before running `idf.py` to avoid a console encoding
warning:

```powershell
$env:PYTHONUTF8 = "1"
```

## Linux and macOS

Clone ESP-IDF and run the installer once, then source the export script in each shell:

```bash
git clone -b v5.4 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
~/esp/esp-idf/install.sh esp32s3
. ~/esp/esp-idf/export.sh
```

## Confirm it works

```bash
idf.py --version
```

If that prints a version, the toolchain is active in this shell. The export step only
affects the current terminal, so run it again in any new one.
