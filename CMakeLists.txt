# Top-level ESP-IDF project file for the Communicator badge firmware.
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(had-badge-mod)

# The LVGL component does not expose the NV3007 display driver via Kconfig, so
# enable it (and its generic-MIPI dependency) build-wide. lv_conf_internal.h
# reads CONFIG_LV_USE_NV3007 when defined.
idf_build_set_property(COMPILE_OPTIONS "-DCONFIG_LV_USE_NV3007=1" APPEND)
