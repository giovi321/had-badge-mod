idf_component_register(
    SRCS "portable/dedup.c" "message.c" "router.c" "backend_meshtastic.c"
    INCLUDE_DIRS "include"
    REQUIRES "mesh" "core" "util")
