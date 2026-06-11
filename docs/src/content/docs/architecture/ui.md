---
title: User interface
description: The layout, theme, and navigation model.
---

The UI is built on LVGL. It uses one font, a single amber accent on a dark background, and
monochrome icons taken from LVGL's built-in symbol glyphs.

## Layout

The screen is 428 by 142 in landscape. Three regions make up the chrome:

- A status sidebar 28 pixels wide on the left. It stops where the bottom bar begins, so its
  height is the screen height minus the bottom bar.
- A function-key bar across the full width of the bottom, 18 pixels tall. It sits under the
  sidebar and runs to the left edge. The five cells are evenly distributed, and labels are
  clipped with an ellipsis so text never runs off the screen.
- A per-app area between the header and the bottom bar, to the right of the sidebar.

The sidebar and the bottom bar live on LVGL's top layer, so they stay in place while the app
screen below them changes.

## Theme

Geometry constants live in `components/ui/include/ui/layout.h`, and the host tests check the
arithmetic (the five bottom-bar cells sum to the screen width, the sidebar height is correct,
and so on). Colors come from `ui/colors.h`. The font is Montserrat at 14 for body text and 18
for titles, exposed through `theme_font_body()` and `theme_font_title()`. Apps use those
accessors rather than setting fonts directly.

## Status sidebar

The sidebar shows battery, mesh, WiFi, and GPS. Each icon is recolored from a state value:
for example the mesh icon lights when the backend is up, and the battery icon picks a color
from the charge level. The mapping functions are pure and host tested.

## Navigation

The launcher is a horizontally scrolling strip of icon tiles. Left and Right move the
selection and scroll the focused tile into view. Enter or F1 (Open) opens the focused app. The
launcher returns to the tile you last opened. The home screen also shows the firmware version
in the top bar, small and left-aligned, for example `v0.10.0`.

Inside an app, Esc or F5 goes back. F5 always shows "Back" so the control is visible.
Function keys F1 to F4 are app specific, and the bottom bar shows their current labels.

Text entry uses a one-line input. In the Messages app, Enter sends and the Up and Down arrows
scroll the chat history while the input keeps focus.
