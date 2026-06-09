/* Single source of truth for every GPIO on the Hackaday 2024 Communicator badge
 * (ESP32-S3). Ported from firmware/badge/hardware/board.py and lvgl_setup.py. */
#ifndef BOARD_PINS_H
#define BOARD_PINS_H

/* --- Display: NV3007 TFT, 142x428 native (used at 428x142 rotated 270) ---- */
#define PIN_LCD_MOSI       21
#define PIN_LCD_SCLK       38
#define PIN_LCD_DC         39
#define PIN_LCD_RST        40
#define PIN_LCD_CS         41
#define PIN_LCD_TE         42
#define PIN_LCD_BACKLIGHT  2        /* PWM, active LOW */
#define LCD_SPI_HOST       SPI2_HOST
#define LCD_SPI_HZ         (80 * 1000 * 1000)
#define LCD_NATIVE_W       142
#define LCD_NATIVE_H       428
#define LCD_OFFSET_X       0
#define LCD_OFFSET_Y       12
#define LCD_BL_DUTY_MAX    1023     /* 10-bit LEDC */

/* --- Radio: SX1262 LoRa (SPI host 3) -------------------------------------- */
#define PIN_RF_MOSI        3
#define PIN_RF_SCLK        8
#define PIN_RF_MISO        9
#define PIN_RF_NSS         17
#define PIN_RF_RST         18
#define PIN_RF_BUSY        15
#define PIN_RF_DIO1        16       /* IRQ */
#define PIN_RF_SW          10       /* RX/TX antenna switch (1 = ?) */
#define RF_SPI_HOST        SPI3_HOST
#define RF_SPI_HZ          (2 * 1000 * 1000)

/* --- Keyboard: TCA8418 I2C matrix controller ------------------------------ */
#define PIN_KBD_SCL        14
#define PIN_KBD_SDA        47
#define PIN_KBD_INT        13       /* active LOW interrupt */
#define PIN_KBD_RST        48
#define KBD_I2C_PORT       0
#define KBD_I2C_ADDR       0x34     /* TCA8418 fixed address */
#define KBD_I2C_HZ         400000

/* --- GPS (optional): ATGM336H NMEA over UART on header J6 ------------------ */
#define PIN_GPS_TX         11       /* ESP TX -> GPS RX (optional) */
#define PIN_GPS_RX         12       /* ESP RX <- GPS TX (NMEA in) */
#define GPS_UART_NUM       1
#define GPS_UART_BAUD      9600

/* --- Misc ----------------------------------------------------------------- */
#define PIN_DEBUG_LED      1        /* active LOW */
#define PIN_BAT_ADC        (-1)     /* unconfirmed on this board; off by default */

#endif /* BOARD_PINS_H */
