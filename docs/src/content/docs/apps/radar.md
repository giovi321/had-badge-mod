---
title: Find my people (Radar)
description: A radar scope that plots every positioned node by range and bearing.
---

Radar is a plan-position ("PPI") scope. You sit at the centre and every node that has shared a
position is drawn as a blip at its range and bearing from you. It turns the distance and
bearing the Nodes and Tracker apps show as text into a single glance — useful for finding the
rest of your group at a festival or on a trail.

You need your own GPS fix to place the blips. Without one the app says so and the scope stays
empty.

## Orientation

The scope is north-up when you are standing still. While you move it switches to heading-up,
turning so your direction of travel points to the top, derived from your GPS course over
ground (the badge has no magnetometer yet). Press F1 to force north-up or heading-up; the
label shows which one the press will switch to.

## Range

Press F2 (Range) to step the distance the outer ring represents: 200 m, 1 km, 5 km, or Auto.
Auto fits the farthest positioned node. A node beyond the current range is pinned to the rim.

## Reading the scope

- The amber dot in the centre is you, with a short tick marking the up direction.
- Each other node is a dot at its range and bearing. The side panel names the nearest node
  with its distance and bearing, and counts how many nodes are on the scope.
- Press F3 (Next) to highlight nodes in turn; the highlighted node is drawn in amber.

The blips come from the same node database the Nodes and Tracker apps use, so a node appears
as soon as it has broadcast a position. When the badge gains a magnetometer the heading will
come from a real compass instead of GPS course, so the scope orients even while you are
standing still.
