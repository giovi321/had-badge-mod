---
title: Host tests
description: Run the portable-logic test suite on a PC, no hardware needed.
---

The protocol, crypto, packet, region, NMEA, settings, node DB, dedup, and UI layout logic is
written as plain C with no ESP-IDF or LVGL includes. It compiles and runs on a PC, which is
the only part of the firmware you can verify without a badge.

## Run with a bundled compiler

The repository includes a runner that uses `zig cc`, a self-contained C compiler installed as
a Python package, so you do not need a system toolchain:

```bash
python -m pip install ziglang
pwsh tools/run_host_tests.ps1
```

It compiles the portable sources together with the test suites and prints a summary. The
suite covers, among others:

- AES against the FIPS-197 known-answer vectors.
- The Meshtastic channel hash, nonce layout, and AES-CTR round trip.
- Packet header build and parse, including the flags byte and channel hint.
- nanopb encode and decode, with a wire-format check against the original codec.
- Region and frequency math.
- NMEA parsing, the settings registry, the node DB, dedup, and the UI layout geometry.

## Run with CMake

If you have gcc or clang and CMake, build the same suite with the provided project:

```bash
cmake -B build-host host_tests
cmake --build build-host
ctest --test-dir build-host
```

## Why it matters

The Meshtastic wire constants are frozen here. A wrong byte in the crypto or packet code does
not crash, it silently breaks interoperability, so these checks run before anything is
flashed. Keep the suite green on every commit.
