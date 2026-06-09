---
title: Tracker
description: Follow a live Meshtastic node, like a GPS tracker, on a compass.
---

Tracker points a compass needle at another Meshtastic node and shows what that node
transmits. Use it to follow a moving node, say a GPS tracker or someone else's badge, as long as that
node broadcasts its position.

This is different from Follow. Follow walks you back along a track you recorded yourself.
Tracker aims at a live node whose position arrives over the mesh and updates as new packets
come in.

## Choosing a node

Two ways to pick the node to follow:

- In the Nodes app, select a node and press F2 (Track). That sets it as the target and opens
  Tracker.
- In Tracker, press F1 (Next node) to cycle through the nodes that have shared a position.

If you have not chosen one, Tracker picks the first node that has a position.

## What it shows

The needle points toward the node. Because the badge has no magnetometer, the heading uses
your GPS course over ground, so the needle is relative to your direction of travel while you
move and falls back to north up when you stop. You need your own GPS fix for the direction
and distance.

Alongside the compass it lists what the node sent:

- Distance and bearing from you to the node.
- The node's own speed and heading, from its position broadcast.
- Temperature and humidity, if the node sends environment telemetry.
- Battery, if the node sends device telemetry.
- How long ago the last update arrived.

## What a tracker needs to transmit

Anything that interoperates with Meshtastic works. The badge reads the standard `Position`
app for location, speed, and heading, and the `Telemetry` app for battery and environment
metrics (temperature, humidity, barometric pressure). A node that only sends text will not
appear here until it also shares a position.
