---
title: Packets
description: A rolling log of recently received mesh packets.
---

Packets shows the last 32 packets the badge received, newest first. Each line has the time,
the sender node id, the port (text, position, info, telemetry, routing), and the signal
strength.

It is a debugging view. If you are not sure whether the radio is hearing anything, open
Packets and watch for new lines. If a specific device should be in range but nothing shows
up, check that your region, channel, and key match it.

The log lives in RAM and resets on reboot.
