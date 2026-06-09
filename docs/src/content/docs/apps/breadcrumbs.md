---
title: Breadcrumbs and Follow
description: Record a GPS track and navigate back along it.
---

These two apps work together. Breadcrumbs records where you go, and Follow points you
back. Both need GPS wired and a fix.

## Breadcrumbs

Open Breadcrumbs and press F1 to start recording. The badge writes the current position to
a file on its storage partition, named from the start time, for example
`trk_20260609-143022.csv`. It samples a point when you move far enough or after a short time
gap, so a stationary badge does not fill the file. A red dot in the status sidebar shows that
a track is recording. Press F1 again to stop.

The file is a simple CSV: a header line, then one row per point with a timestamp, latitude,
longitude, altitude, and satellite count.

## Follow

Follow loads the most recent track and shows a needle pointing toward the next waypoint on
the way back to the start. It also shows the distance to that waypoint and which one you are
on.

The badge has no magnetometer, so heading comes from GPS course over ground. While you are
moving the needle is relative to your direction of travel. When you stop, there is no course,
so the needle falls back to north up and the app says so. As you reach each waypoint it
advances to the previous one, walking you back to where the track began.
