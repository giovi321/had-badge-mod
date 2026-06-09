# Standalone host-test build (no ESP-IDF). Requires any C11 compiler + CMake.
#
#   cmake -B build-host host_tests && cmake --build build-host && ctest --test-dir build-host
#
# tools/run_host_tests.ps1 does the same with a bundled `zig cc` (no system toolchain).
cmake_minimum_required(VERSION 3.16)
project(had_badge_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
get_filename_component(ROOT ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)

# Portable sources only (no ESP-IDF/LVGL deps). core/settings_nvs.c is excluded.
set(PORTABLE
    ${ROOT}/components/mesh/aes.c
    ${ROOT}/components/mesh/regions.c
    ${ROOT}/components/mesh/mesh_crypto.c
    ${ROOT}/components/mesh/packet.c
    ${ROOT}/components/mesh/meshtastic_pb.c
    ${ROOT}/components/mesh/nanopb/pb_common.c
    ${ROOT}/components/mesh/nanopb/pb_encode.c
    ${ROOT}/components/mesh/nanopb/pb_decode.c
    ${ROOT}/components/mesh/generated/meshtastic.pb.c
    ${ROOT}/components/core/eventbus.c
    ${ROOT}/components/core/settings.c
    ${ROOT}/components/core/nodedb.c
    ${ROOT}/components/net/portable/dedup.c
    ${ROOT}/components/ui/portable/theme_map.c
    ${ROOT}/components/util/nmea.c)

file(GLOB TESTS ${CMAKE_CURRENT_LIST_DIR}/*.c)

add_executable(host_tests ${PORTABLE} ${TESTS})
target_include_directories(host_tests PRIVATE
    ${ROOT}/components/mesh/include
    ${ROOT}/components/mesh/nanopb
    ${ROOT}/components/mesh/generated
    ${ROOT}/components/core/include
    ${ROOT}/components/net/include
    ${ROOT}/components/ui/include
    ${ROOT}/components/util/include
    ${CMAKE_CURRENT_LIST_DIR})
target_compile_options(host_tests PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare)

enable_testing()
add_test(NAME host_tests COMMAND host_tests)
