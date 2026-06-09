idf_component_register(
    SRCS "radio_task.c"
    INCLUDE_DIRS "include"
    REQUIRES "drivers" "net" "mesh" "core" "bsp" "esp_timer")
