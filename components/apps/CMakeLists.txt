idf_component_register(
    SRCS "app_manager.c" "launcher.c" "messages.c" "nodes.c"
         "settings_app.c" "gps_app.c" "diag.c" "track.c" "follow.c" "packets.c"
         "tracker.c"
    INCLUDE_DIRS "include"
    REQUIRES "ui" "core" "net" "mesh" "services" "drivers" "util" "lvgl" "bsp" "ble"
             "esp_timer" "nvs_flash")
