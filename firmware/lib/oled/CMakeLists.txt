cmake_minimum_required(VERSION 3.13)

## COMMENT THIS LINE FOR LOCAL SDK
# set(PICO_SDK_FETCH_FROM_GIT on)
set(PICO_SDK_PATH "~/repo/pico-sdk")
include("${PROJECT_SOURCE_DIR}/pico_sdk_import.cmake")

pico_sdk_init()

add_library(oled ssd1306.c)

target_link_libraries(oled pico_stdlib hardware_i2c)

target_include_directories(oled INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(oled PUBLIC "${PROJECT_BINARY_DIR}/cfg")
