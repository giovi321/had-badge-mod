---
title: WiFi and web UI
description: Connect to a network or host a hotspot, and configure the badge from a browser.
---

WiFi is off by default. Turn it on in Settings, under the WiFi group.

## Modes

Set `wifi_mode` to one of:

- `sta`: join an existing network. Set `wifi_ssid` and `wifi_pass`.
- `ap`: host a hotspot. Set `ap_ssid` and, for a protected network, an 8 character or longer
  `ap_pass`. An empty password makes it open.
- `off`: WiFi disabled.

Reboot after changing the mode. The serial log prints the IP address when the link comes up.
The WiFi icon in the sidebar reflects the state.

## Web UI

When WiFi is up and `web_enabled` is on, the badge serves a web interface on its IP address.

- `/` is a settings form. It lists every setting, grouped, and writes changes back to the
  badge. It is the same schema the on-device Settings app uses, so the two stay in sync.
- `/diag` shows the diagnostics: node id, radio config, peer count, packet counters, and the
  last signal.

Radio changes made in the web form take effect after a reboot.

The form prefills current values, including passwords, so treat the badge's network as
trusted. If you host an open hotspot, anyone who joins can read and change settings.
