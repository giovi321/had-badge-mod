idf_component_register(
    SRCS "eventbus.c" "settings.c" "settings_json.c" "nodedb.c" "settings_nvs.c"
    INCLUDE_DIRS "include"
    REQUIRES "nvs_flash")
