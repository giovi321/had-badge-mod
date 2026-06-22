---
title: Board and pins
description: The Communicator badge hardware and its GPIO map.
---

The target is the Hackaday 2024 Supercon Communicator badge.

| Part | Detail |
|------|--------|
| MCU | ESP32-S3-WROOM, 8 MB octal PSRAM, 16 MB flash |
| Display | NV3007 TFT, 428x142 used rotated, SPI at 80 MHz, RGB565 byte swapped |
| Radio | Semtech SX1262 LoRa, TCXO on DIO3 at 1.7 V, DIO2 plus GPIO10 RF switch |
| Keyboard | TCA8418 I2C matrix controller, interrupt driven |
| GPS (optional) | ATGM336H NMEA over UART on header J6 |
| Power | LiPo with MCP73831 charger, backlight PWM on GPIO2 |

Every pin lives in one header, `components/bsp/include/board_pins.h`, so there is a single
place to change wiring.

## GPIO map

| Function | Pins |
|----------|------|
| Display SPI | MOSI 21, SCLK 38, DC 39, RST 40, CS 41, TE 42, backlight 2 |
| Radio SPI | NSS 17, MOSI 3, SCLK 8, MISO 9, RST 18, BUSY 15, DIO1 16, RF switch 10 |
| Keyboard I2C | SCL 14, SDA 47, INT 13, RST 48 |
| GPS UART (J6) | ESP RX 12 (from GPS TX), ESP TX 11 (to GPS RX) |

## Notes

The display and radio sit on separate SPI hosts (the display on SPI2, the radio on SPI3),
so they do not share a bus.

The GPS header J6 is a 4-pin expansion connector, not fitted from the factory. Its pin order is
3V3, IO11 (ESP TX), IO12 (ESP RX), GND. The IO12 pad is easily misread as "IO10"; GPIO10 is the
LoRa antenna switch and is not on J6.

The badge has no magnetometer and no real-time clock. The compass features derive heading
from GPS course over ground, and the clock is set from the GPS time when a fix is available.

The battery sense pin is not confirmed on the stock board, so battery reporting is off by
default. Set the ADC pin and divider in the firmware once you know the schematic value.
