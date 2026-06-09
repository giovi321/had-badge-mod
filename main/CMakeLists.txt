idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES drivers radio services ui apps core net mesh util ble
             esp_timer esp_pm nvs_flash)
