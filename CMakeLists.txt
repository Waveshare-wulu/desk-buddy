cmake_minimum_required(VERSION 3.16)
option(DESK_BUDDY_DEVICE_SMOKE "Run the on-device UI regression test at startup" OFF)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

add_compile_options(
    "$<$<AND:$<COMPILE_LANG_AND_ID:CXX,GNU>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,15.0>>:-Wno-error=deprecated-variadic-comma-omission>"
)

project(desk_buddy)
