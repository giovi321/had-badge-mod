---
title: Diagnostics
description: Live mesh and system statistics.
---

The Diagnostics app shows live values that help confirm the radio is working and the badge is
configured the way you expect.

It reports:

- The node id.
- The region, preset, frequency, and spreading factor.
- The channel name and the sync word.
- The hop limit and whether relaying is on.
- The number of known peers.
- The count of received and transmitted packets.
- The last received signal strength and signal-to-noise ratio.
- Uptime and free heap.

The receive and transmit counters are the quickest way to tell whether the radio is doing
anything. If you send a message and the transmit count goes up, the firmware reached the
radio. If another device is nearby and the receive count stays at zero, check the region,
channel, and key against that device.
