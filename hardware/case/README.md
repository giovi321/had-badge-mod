# Communicator GPS case

A 3D-printable case for the Hackaday Communicator badge with a bay for the ATGM336H GPS module
and its wiring to the J6 expansion header. It is a remix of the stock case, so the badge still
fits the same way; the change is the room it makes for the receiver.

## Files

- `communicator-gps-case.stl` — the printable model. It was drawn in SketchUp and exported as a
  binary STL (2,846 triangles).

## Printing

The model is in millimetres, so slice it at full size with no scaling. Its outer footprint is
about 148 x 175 mm and the walls are 8.5 mm tall.

There are no print settings baked into the STL, so slice it for your own printer and filament.
PLA or PETG is fine for a badge you carry around indoors. Check the orientation in your slicer
preview before you decide whether you need supports.

## GPS

The bay is sized for the common ATGM336H breakout. Solder it to the J6 header (TX to IO12, RX to
IO11, plus 3V3 and GND), then turn GPS on in Settings. J6 is not fitted from the factory, so you
add the header or wires yourself. The wiring and the IO12-vs-IO10 silkscreen trap are covered in
the [GPS app docs](https://giovi321.github.io/had-badge-mod/apps/gps/).
