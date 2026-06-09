idf_component_register(
    SRCS "app_manager.c" "launcher.c" "messages.c" "nodes.c"
         "settings_app.c" "gps_app.c"
    INCLUDE_DIRS "include"
    REQUIRES "ui" "core" "net" "mesh" "services" "drivers" "lvgl" "bsp")
