---
title: Messages
description: Send and receive Meshtastic text messages.
---

Messages is the main app. It shows the chat as bubbles, with your messages on the right in
amber and received messages on the left in grey, labelled with the sender.

## Sending

The input is a single line at the bottom of the screen, focused when you open the app. Type a
message and press Enter, or press F1 (Send). Your message is sent on the current channel and
appears as a bubble once the radio confirms it.

## Reading and scrolling

Incoming messages are added to the history as they arrive, even if the Messages app is not
open. Use the Up and Down arrows to scroll the history. The input stays focused, so you can
keep typing without leaving the scroll position.

## Direct messages and acks

By default messages go to the whole channel. To message one node, open the Nodes app, select
a node, and press F1 or Enter. That sets the recipient and switches to Messages, which shows
"To: <name>" at the top. Direct messages ask for a delivery acknowledgement, and when it
comes back the sent bubble gets a check mark. Press F2 to switch back to broadcast.

## History across boots

The chat history is saved to NVS and reloaded on boot. The number of messages kept is set by
the `msg_keep` value in Settings, under the Messages group. The default is 50. Set it to 0 to
disable saving.

The firmware writes the history when you leave the app and on a short timer while messages
arrive, so a reboot keeps your recent conversation.
