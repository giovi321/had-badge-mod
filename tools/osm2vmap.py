#!/usr/bin/env python3
"""Convert an OpenStreetMap extract into a .vmap file for the badge radar overlay.

The badge draws a vector-map background behind the radar blips from a compact
binary file (`/spiffs/map.vmap`). This tool turns a GeoJSON export of an area's
roads and water into that binary.

Workflow
--------
1. Grab the data for your area from Overpass Turbo (https://overpass-turbo.eu),
   "Export -> download as GeoJSON". A query that gets roads + water:

       [out:json][timeout:60];
       (
         way["highway"]({{bbox}});
         way["natural"="water"]({{bbox}});
         way["natural"="coastline"]({{bbox}});
         way["waterway"]({{bbox}});
         relation["natural"="water"]({{bbox}});
       );
       out geom;

   (Overpass Turbo emits GeoJSON directly; or run any .osm through
   `osmtogeojson`.)

2. Convert:

       python tools/osm2vmap.py --geojson area.geojson --out map.vmap

3. Upload `map.vmap` to the badge at http://<badge-ip>/map and toggle the
   overlay with F4 in the Radar app.

Keep the area regional -- a city centre at a few kilometres across is plenty for
radar ranges (<= 5 km). Use --simplify and --bbox to keep the file small
(a soft budget of ~1-2 MB leaves room on the 9.6 MB SPIFFS).

Binary format (little-endian; must match components/util/vmap.h):
  Header  (28 B): 'VMAP', u8 version=1, u8 flags=0, u16 reserved,
                  i32 global bbox[min_lat,min_lon,max_lat,max_lon] (e7), u32 count
  Feature (20 B + 8*N): u8 class(1=road,2=water), u8 reserved, u16 point_count,
                  i32 bbox[4] (e7), then N*(i32 lat_e7, i32 lon_e7)
"""
import argparse
import json
import math
import struct
import sys

MAGIC = b"VMAP"
VERSION = 1
CLASS_ROAD = 1
CLASS_WATER = 2
MAX_POINTS = 256        # must equal VMAP_MAX_POINTS
I32_MAX = 2**31 - 1
I32_MIN = -2**31


def classify(props):
    """Return CLASS_ROAD / CLASS_WATER / None from OSM tags in GeoJSON props."""
    if not isinstance(props, dict):
        return None
    if props.get("highway"):
        return CLASS_ROAD
    if (props.get("natural") in ("water", "coastline")
            or props.get("waterway")
            or props.get("water")):
        return CLASS_WATER
    return None


def polylines(geom):
    """Yield lists of (lat, lon) for every line in a GeoJSON geometry."""
    if not geom:
        return
    t = geom.get("type")
    c = geom.get("coordinates")
    if t == "LineString":
        yield [(p[1], p[0]) for p in c]
    elif t == "MultiLineString":
        for line in c:
            yield [(p[1], p[0]) for p in line]
    elif t == "Polygon":
        for ring in c:
            yield [(p[1], p[0]) for p in ring]
    elif t == "MultiPolygon":
        for poly in c:
            for ring in poly:
                yield [(p[1], p[0]) for p in ring]
    # Points / GeometryCollection: nothing to draw


def simplify(pts, tol_m):
    """Douglas-Peucker in a local metres frame. Keeps endpoints."""
    n = len(pts)
    if n < 3 or tol_m <= 0:
        return pts
    lat0 = pts[n // 2][0]
    kx = 111320.0 * math.cos(math.radians(lat0))   # metres per degree lon
    ky = 111320.0                                   # metres per degree lat

    def xy(p):
        return (p[1] * kx, p[0] * ky)

    keep = [False] * n
    keep[0] = keep[-1] = True
    stack = [(0, n - 1)]
    while stack:
        a, b = stack.pop()
        if b <= a + 1:
            continue
        ax, ay = xy(pts[a])
        bx, by = xy(pts[b])
        dx, dy = bx - ax, by - ay
        seg2 = dx * dx + dy * dy
        far_d, far_i = -1.0, -1
        for i in range(a + 1, b):
            cx, cy = xy(pts[i])
            if seg2 == 0.0:
                d = math.hypot(cx - ax, cy - ay)
            else:
                t = ((cx - ax) * dx + (cy - ay) * dy) / seg2
                t = max(0.0, min(1.0, t))
                d = math.hypot(cx - (ax + t * dx), cy - (ay + t * dy))
            if d > far_d:
                far_d, far_i = d, i
        if far_d > tol_m:
            keep[far_i] = True
            stack.append((a, far_i))
            stack.append((far_i, b))
    return [p for p, k in zip(pts, keep) if k]


def split(pts, cap):
    """Split a polyline into chunks of <= cap points, sharing the seam vertex."""
    if len(pts) <= cap:
        return [pts]
    out = []
    i = 0
    while i < len(pts) - 1:
        out.append(pts[i:i + cap])
        i += cap - 1
    return out


def bbox_of(pts):
    lats = [p[0] for p in pts]
    lons = [p[1] for p in pts]
    return min(lats), min(lons), max(lats), max(lons)


def intersects(a, b):
    return not (a[2] < b[0] or a[0] > b[2] or a[3] < b[1] or a[1] > b[3])


def e7(deg):
    return max(I32_MIN, min(I32_MAX, int(round(deg * 1e7))))


def build_features(fc, filt_bbox, tol_m, cap):
    """Return [(class, [(lat,lon),...]), ...] from a GeoJSON FeatureCollection."""
    feats = fc.get("features", []) if isinstance(fc, dict) else []
    out = []
    for f in feats:
        cls = classify(f.get("properties"))
        if cls is None:
            continue
        for line in polylines(f.get("geometry")):
            if len(line) < 2:
                continue
            bb = bbox_of(line)  # (min_lat,min_lon,max_lat,max_lon)
            if filt_bbox and not intersects(
                    (bb[0], bb[1], bb[2], bb[3]),
                    (filt_bbox[1], filt_bbox[0], filt_bbox[3], filt_bbox[2])):
                continue
            simp = simplify(line, tol_m)
            for chunk in split(simp, cap):
                if len(chunk) >= 2:
                    out.append((cls, chunk))
    return out


def write_vmap(path, features):
    body = bytearray()
    count = 0
    g = [I32_MAX, I32_MAX, I32_MIN, I32_MIN]  # min_lat,min_lon,max_lat,max_lon
    for cls, pts in features:
        ept = [(e7(la), e7(lo)) for la, lo in pts]
        if len(ept) < 2:
            continue
        if len(ept) > MAX_POINTS:
            raise ValueError("feature exceeds MAX_POINTS after split")
        lats = [p[0] for p in ept]
        lons = [p[1] for p in ept]
        fb = (min(lats), min(lons), max(lats), max(lons))
        body += struct.pack("<BBHiiii", cls, 0, len(ept), fb[0], fb[1], fb[2], fb[3])
        for la, lo in ept:
            body += struct.pack("<ii", la, lo)
        g[0] = min(g[0], fb[0]); g[1] = min(g[1], fb[1])
        g[2] = max(g[2], fb[2]); g[3] = max(g[3], fb[3])
        count += 1
    if count == 0:
        g = [0, 0, 0, 0]
    header = struct.pack("<4sBBHiiiiI", MAGIC, VERSION, 0, 0,
                         g[0], g[1], g[2], g[3], count)
    with open(path, "wb") as f:
        f.write(header)
        f.write(body)
    return count, len(header) + len(body)


def selfcheck(path, expect_count):
    """Re-parse the written file and confirm structure + feature count."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != MAGIC or data[4] != VERSION:
        raise ValueError("bad header")
    count = struct.unpack_from("<I", data, 24)[0]
    if count != expect_count:
        raise ValueError("count mismatch")
    off = 28
    seen = 0
    while seen < count:
        cls, _res, npc = struct.unpack_from("<BBH", data, off)
        off += 20 + 8 * npc
        if off > len(data):
            raise ValueError("truncated feature")
        if npc < 2 or npc > MAX_POINTS:
            raise ValueError("bad point count")
        seen += 1
    if off != len(data):
        raise ValueError("trailing bytes")
    return True


def main(argv=None):
    ap = argparse.ArgumentParser(description="OSM GeoJSON -> .vmap for the badge radar overlay")
    ap.add_argument("--geojson", required=True, help="input GeoJSON FeatureCollection")
    ap.add_argument("--out", default="map.vmap", help="output .vmap (default: map.vmap)")
    ap.add_argument("--simplify", type=float, default=2.0,
                    help="Douglas-Peucker tolerance in metres (default: 2)")
    ap.add_argument("--bbox", default=None,
                    help="optional minlon,minlat,maxlon,maxlat filter")
    ap.add_argument("--max-points", type=int, default=MAX_POINTS,
                    help="max points per feature (<= %d)" % MAX_POINTS)
    args = ap.parse_args(argv)

    cap = max(2, min(args.max_points, MAX_POINTS))
    filt = None
    if args.bbox:
        try:
            filt = [float(x) for x in args.bbox.split(",")]
            if len(filt) != 4:
                raise ValueError
        except ValueError:
            ap.error("--bbox must be minlon,minlat,maxlon,maxlat")

    with open(args.geojson, "r", encoding="utf-8") as f:
        fc = json.load(f)

    features = build_features(fc, filt, args.simplify, cap)
    roads = sum(1 for c, _ in features if c == CLASS_ROAD)
    water = sum(1 for c, _ in features if c == CLASS_WATER)
    count, size = write_vmap(args.out, features)
    selfcheck(args.out, count)

    print("wrote %s: %d features (%d road, %d water), %d bytes"
          % (args.out, count, roads, water, size))
    if count == 0:
        print("warning: no road/water features found -- check the GeoJSON tags",
              file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
