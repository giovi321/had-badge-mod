set(ui_srcs "portable/theme_map.c" "theme.c" "frame.c" "sidebar.c" "menubar.c"
    "launcher_strip.c" "icons.c" "map_canvas.c")
if(CONFIG_BADGE_FONT_LATIN1)
    list(APPEND ui_srcs "fonts/lv_font_montserrat_latin1_14.c"
                        "fonts/lv_font_montserrat_latin1_18.c")
endif()
idf_component_register(
    SRCS ${ui_srcs}
    INCLUDE_DIRS "include"
    REQUIRES "lvgl" "core" "net" "mesh" "util" "bsp")
