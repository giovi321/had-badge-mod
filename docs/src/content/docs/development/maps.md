---
title: Offline maps
description: Build a .vmap vector map from OpenStreetMap and upload it for the radar overlay.
---

The [Radar](/apps/radar/) app can draw an offline vector map — roads and water — behind the
blips. The map is a compact binary file (`.vmap`) you build on a PC from OpenStreetMap data and
upload to the badge over Wi-Fi. It lives on the badge's SPIFFS storage at `/spiffs/map.vmap`;
the radar reads it directly and projects it onto the scope with the same range/bearing maths it
uses for the blips, so map and people stay aligned.

There is no live download on the badge — everything is offline. A regional extract (a city
centre a few kilometres across) is plenty for radar ranges, which top out at 5 km.

## 1. Get the data from OpenStreetMap

The simplest source is [Overpass Turbo](https://overpass-turbo.eu). Pan/zoom to your area, paste
this query (it pulls roads and water for the current map view), **Run**, then
**Export → download as GeoJSON**:

```overpassql
[out:json][timeout:60];
(
  way["highway"]({{bbox}});
  way["natural"="water"]({{bbox}});
  way["natural"="coastline"]({{bbox}});
  way["waterway"]({{bbox}});
  relation["natural"="water"]({{bbox}});
);
out geom;
```

Any tool that produces GeoJSON works too — for example running a downloaded `.osm` extract
through [`osmtogeojson`](https://github.com/tyrasd/osmtogeojson).

## 2. Convert to .vmap

```sh
python tools/osm2vmap.py --geojson area.geojson --out map.vmap
```

Useful flags:

- `--simplify <metres>` — Douglas–Peucker tolerance (default `2`). Larger values drop more
  vertices and shrink the file.
- `--bbox minlon,minlat,maxlon,maxlat` — keep only features intersecting this box, to trim a
  large export down to your area.

The tool classifies OSM tags into two layers (`highway=*` → roads, `natural=water|coastline` /
`waterway=*` / `water=*` → water), simplifies each way, splits long ways so no feature exceeds
256 points, and writes the binary. It prints the feature counts and size and re-parses its own
output as a sanity check. The tool needs only the Python standard library.

Keep the result small — a soft budget of **1–2 MB** leaves plenty of room on the ~9.6 MB SPIFFS.
If a file is too big, raise `--simplify` or narrow `--bbox`.

## 3. Upload to the badge

Join the badge's Wi-Fi / web UI (see [WiFi and web UI](/connectivity/wifi/)), open
**`http://<badge-ip>/map`**, pick your `map.vmap`, and upload. The badge streams it to a temp
file, validates the header, and atomically swaps it in — a bad or interrupted upload leaves the
previous map untouched. Then open Radar and press **F4** to toggle the overlay.

Uploading a new file replaces the old one. The map survives reboots but, like all SPIFFS data,
is wiped if the partition table is ever changed (a one-time cable re-flash event).

## File format reference

`.vmap` is little-endian throughout. Coordinates are fixed-point 1e-7 degrees
(`int32 = round(deg × 1e7)`). The canonical definition lives in
`components/util/vmap.h`; the reader and host tests are in `components/util/vmap.c` and
`host_tests/test_map.c`.

**Header — 28 bytes**

| bytes | field |
| ----- | ----- |
| 4 | magic `VMAP` |
| 1 | version (`1`) |
| 1 | flags (`0`) |
| 2 | reserved |
| 16 | global bbox: `min_lat, min_lon, max_lat, max_lon` (int32 e7) |
| 4 | feature count (uint32) |

**Each feature — 20-byte prefix + 8 × N point bytes**

| bytes | field |
| ----- | ----- |
| 1 | class (`1` = road, `2` = water) |
| 1 | reserved |
| 2 | point count N (uint16, 2–256) |
| 16 | feature bbox (int32 e7) |
| 8 × N | points: `lat_e7, lon_e7` pairs (int32), in polyline order |

The per-feature bounding box lets the badge skip out-of-view features without reading their
points, so it only projects the geometry near you. Unknown class bytes are skipped, so the
format can grow new layers without breaking older firmware.
