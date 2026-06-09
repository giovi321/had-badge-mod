---
title: Nodes
description: The list of mesh nodes the badge has heard.
---

Nodes lists the other devices the badge has received packets from. Each row shows the node
name, the last signal-to-noise ratio, and how long ago it was last heard.

A node appears here after the badge decodes a packet from it, which includes its NodeInfo
broadcast. The name is the long name from NodeInfo when available, otherwise the short name,
otherwise the node id in `!aabbccdd` form.

When the badge has a GPS fix and a node has shared its position, the row also shows the
distance and a compass direction to that node. If a node has sent telemetry, the row shows
its battery level.

Use the arrow keys to move through the list. Press F1 or Enter on a node to start a direct
message to it.

The node table is held in RAM with a fixed capacity and evicts the least recently heard node
when it is full.
