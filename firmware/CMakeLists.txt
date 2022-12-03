cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH

## COMMENT AND EDIT THESE LINES TO SWITCH
## BETWEEN LOCAL OR GIT SDK
# set(PICO_SDK_FETCH_FROM_GIT on)
set(PICO_SDK_PATH "~/repo/pico-sdk")
set(PICO_BOARD pico)

include(pico_sdk_import.cmake)

project(pico_post_fw
    VERSION 0.1.0
)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

## USER CONFIGURABLE PARAMS
set(OLED_ADDR      0x3C)
set(OLED_WIDTH     128)
set(OLED_HEIGHT    32)
set(OLED_DRAW_MODE 0x08)

# output configuration
configure_file("cfg/proj.h.in" "cfg/proj.h")
configure_file("cfg/pins.h.in" "cfg/pins.h")

list(APPEND PROJ_INCS "${PROJECT_BINARY_DIR}/cfg")
list(APPEND PROJ_INCS "${PROJECT_SOURCE_DIR}/include")

# import proj libraries
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/oled")
list(APPEND PROJ_LIBS oled)

# finalize executable
add_executable(pico_post_fw
    "${PROJECT_SOURCE_DIR}/src/main.c"
)

pico_generate_pio_header(pico_post_fw
    "${PROJECT_SOURCE_DIR}/src/fastread.pio"
)

# include libs
target_link_libraries(pico_post_fw PUBLIC
    pico_stdlib
    pico_multicore
    hardware_pio
    ${PROJ_LIBS}
)

pico_enable_stdio_usb(pico_post_fw 1)
pico_enable_stdio_uart(pico_post_fw 0)

# find includes
target_include_directories(pico_post_fw PUBLIC
    "${PROJECT_BINARY_DIR}"
    ${PROJ_INCS}
)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(pico_post_fw)
