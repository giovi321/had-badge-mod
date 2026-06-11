---
title: Meshtastic stack
description: How the firmware speaks the Meshtastic LoRa protocol.
---

The badge is a real node on a Meshtastic mesh. The on-air format is implemented directly and
checked against the values a stock Meshtastic device uses, so packets interoperate.

## Protobufs

Message bodies use the official Meshtastic protobufs (`Data`, `Position`, `User`, and
`Telemetry` with device and environment metrics) generated with nanopb. The proto
definitions live in `components/mesh/proto/meshtastic.proto`, and the generated C is in
`components/mesh/generated/`. Field numbers and types match upstream, so the wire bytes are
the same.

Received `Position` packets contribute the node's location, and also its ground speed and
heading, which the Tracker app uses to follow a moving node. `Telemetry` packets contribute
battery level and, when present, environment readings such as temperature and humidity.

## Crypto

Channels use AES-CTR with the channel pre-shared key. The defaults are pinned and frozen as
host tests:

- The default channel hash for `LongFast` is `0x08`.
- The 16-byte nonce is `packet_id` little-endian, then four zero bytes, then `from` little
  endian, then four zero bytes. It is the initial big-endian 128-bit counter.
- The default PSK expands the one-byte shorthand `0x01` to the published 16-byte key.

## Channels

The badge can run several channels at once — a primary plus a few secondaries, each a name and
a key. They share one frequency and are told apart by the one-byte channel hash and the key, so
a received frame is decoded by trying each configured channel in turn. The channel-decode helper
is portable and host-tested. Outgoing messages use the channel selected in the Messages app, and
each stored message remembers which channel it belongs to.

## Packet format

The on-air header is 16 bytes, little endian: `to`, `from`, and `id` as 32-bit values, then
the flags byte, the channel hash, `next_hop`, and `relay_node`. The default LongFast flags
value is `0x63`. The LoRa sync word is `0x2B`, not the legacy `0x12` the BadgeNet build used.

Received packets are de-duplicated by the pair `(from, id)`. Rebroadcast is opt in and off
by default, so the badge behaves as a handheld client rather than a router unless you turn
relaying on.

## Region and preset

The region and modem preset set the frequency and the spreading factor. Two values are
pinned by host tests as a sanity check: EU_868 LongFast resolves to 869.525 MHz, and US
LongFast slot 19 resolves to 906.875 MHz.

## Receive and transmit path

The DIO1 interrupt signals the radio task, which reads the FIFO, runs the packet through
dedup, decryption, and the nanopb decode, then publishes a message on the event bus. The
Messages app records it whether or not that app is open.

On transmit the radio task does listen-before-talk with channel activity detection. It waits
on DIO1 for the CAD result rather than polling, backs off a bounded number of times, and
then sends. This avoids the tight spin that once starved the MicroPython event loop.
