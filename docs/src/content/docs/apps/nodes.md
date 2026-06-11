---
title: Nodes
description: The list of mesh nodes the badge has heard.
---

Nodes lists the other devices the badge has received packets from. Each row's first line shows
the node name, the last signal-to-noise ratio, and how long ago it was last heard. If the node
has sent telemetry, the battery level is added to the end of that line.

A node appears here after the badge decodes a packet from it, which includes its NodeInfo
broadcast. The name is the long name from NodeInfo when available, otherwise the short name,
otherwise the node id in `!aabbccdd` form.

## Position, distance, and bearing

When a node has shared its position, a second line shows its reported coordinates next to a GPS
marker. This appears whether or not the badge itself has a fix.

When the badge also has its own GPS fix, that line gains the distance to the node and the
bearing as degrees plus a compass point, for example `1.4km  312 NW`. A small needle on the
right of the row points the same bearing. While you are moving the needle points relative to
your direction of travel, so it shows the way to steer; when you are still it points relative
to north.

## Keys

Use the arrow keys to move through the list. With a node selected:

- F1 (Message) starts a direct message to it, switching to the Messages app with that node as
  the recipient.
- F2 (Track) opens the [Tracker](/had-badge-mod/apps/tracker/) app pointed at that node, for
  following its live position, speed, and any temperature it reports.
- F3 (Announ.) broadcasts this badge's own node info immediately, so the others learn about it
  without waiting for the periodic announce.

The node table is held in RAM with a fixed capacity and evicts the least recently heard node
when it is full.
