---
title: Case and enclosure
description: A 3D-printable badge case with a bay for the GPS module.
---

The `hardware/case/` folder holds a 3D-printable case for the Communicator badge. It is a remix
of the stock case that makes room for an ATGM336H GPS module and its wires to the J6 header, so
you can carry the badge with GPS attached instead of leaving the receiver loose.

## The file

`hardware/case/communicator-gps-case.stl` is the printable model. It was drawn in SketchUp and
exported as a binary STL with 2,846 triangles.

[Download the STL](https://github.com/giovi321/had-badge-mod/raw/main/hardware/case/communicator-gps-case.stl)

## Printing

The model is in millimetres, so slice it at full size and do not scale it. The outer footprint
is about 148 x 175 mm and the walls stand 8.5 mm tall.

The STL carries no print settings of its own, so slice it for your own printer and filament. PLA
or PETG both work for a badge you carry indoors. Look at the orientation in your slicer preview
before you commit to supports.

## Fitting the GPS

The bay is sized for the common ATGM336H breakout. Solder the module to the J6 expansion header,
then enable GPS in Settings and reboot. The pinout (TX to IO12, RX to IO11, plus 3V3 and GND),
and the IO12 pad that is easy to misread as "IO10", are in the
[GPS app](/had-badge-mod/apps/gps/) page. J6 is not populated from the factory, so you solder
the header or wires yourself.
