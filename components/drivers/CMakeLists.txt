idf_component_register(
    SRCS "display_nv3007.c" "backlight.c" "kbd_tca8418.c" "radio_sx1262.c"
         "gps_uart.c" "battery_adc.c" "power_mgmt.c"
    INCLUDE_DIRS "include"
    REQUIRES "bsp" "lvgl" "esp_lcd" "esp_driver_spi" "esp_driver_gpio"
             "esp_driver_i2c" "esp_driver_uart" "esp_driver_ledc"
             "esp_adc" "esp_pm" "esp_timer")
