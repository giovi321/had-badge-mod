idf_component_register(
    SRCS "status_svc.c" "mesh_svc.c" "time_svc.c" "battery_svc.c" "gps_svc.c"
         "track_svc.c" "wifi_svc.c" "web_svc.c"
    INCLUDE_DIRS "include"
    REQUIRES "core" "net" "drivers" "ui" "bsp" "util" "radio" "lvgl"
             "esp_timer" "spiffs" "esp_http_server")
