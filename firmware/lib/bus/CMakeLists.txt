cmake_minimum_required(VERSION 3.13)

## COMMENT THIS LINE FOR LOCAL SDK
# set(PICO_SDK_FETCH_FROM_GIT on)
set(PICO_SDK_PATH "~/repo/pico-sdk")
include("${PROJECT_SOURCE_DIR}/pico_sdk_import.cmake")

pico_sdk_init()

add_library(isa_bus bus.c)

target_link_libraries(isa_bus pico_stdlib)

target_include_directories(isa_bus INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(isa_bus PUBLIC "${PROJECT_BINARY_DIR}/cfg")
