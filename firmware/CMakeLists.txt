cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH

## UNCOMMENT THIS LINE TO SWITCH TO GIT-FETCHED SDK
# set(PICO_SDK_FETCH_FROM_GIT on)

# you should be setting this from env variables, really
# set(PICO_SDK_PATH "~/repo/pico-sdk")
set(PICO_BOARD pico)

include(pico_sdk_import.cmake)

project(pico_post_fw
    VERSION 0.4.2
)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

## USER CONFIGURABLE PARAMS
set(CALIBADJ_5V 1.030)
set(CALIBADJ_12V 1.017)
set(CALIBADJ_N12V 0.836)

list(APPEND PROJ_DEFS PICOPOST_STANDBY_TIMER=15)

option(PICOPOST_USB_FALLBACK "Enable serial output if display not found" OFF)
option(PICOPOST_RESET_HDLR "[EXPERIMENTAL] Enable reset pulse detection using PIO engines" ON)
option(PICOPOST_SUPPORT_REV5 "[EXPERIMENTAL] Enable support for older Rev5 PCB" OFF)
option(PICOPOST_NEGATIVE12_APPROX "Turn off to see how bad my original calculations are" ON)

if(PICOPOST_USB_FALLBACK)
    list(APPEND PROJ_DEFS PICOPOST_USB_FALLBACK)
endif()
if(PICOPOST_RESET_HDLR)
    list(APPEND PROJ_DEFS PICOPOST_RESET_HDLR)
endif()
if(PICOPOST_SUPPORT_REV5)
    list(APPEND PROJ_DEFS PICOPOST_SUPPORT_REV5)
endif()

# output configuration
configure_file("cfg/proj.h.in" "cfg/proj.h")
configure_file("cfg/pins.h.in" "cfg/pins.h")
configure_file("cfg/calib.h.in" "cfg/calib.h")

list(APPEND PROJ_INCS "${PROJECT_BINARY_DIR}/cfg")
list(APPEND PROJ_INCS "${PROJECT_SOURCE_DIR}/include")

# import proj libraries
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/pico-oled")
list(APPEND PROJ_LIBS pico_oled)
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/voltmon")
list(APPEND PROJ_LIBS volt_mon)
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/gpioexp")
list(APPEND PROJ_LIBS gpio_exp)

# finalize executable
add_executable(pico_post_fw
    "${PROJECT_SOURCE_DIR}/src/logic.cpp"
    "${PROJECT_SOURCE_DIR}/src/ui.cpp"
    "${PROJECT_SOURCE_DIR}/src/app.cpp"
    "${PROJECT_SOURCE_DIR}/src/main.cpp"
)

pico_generate_pio_header(pico_post_fw
    "${PROJECT_SOURCE_DIR}/src/fastread.pio"
)

# include libs
target_link_libraries(pico_post_fw PUBLIC
    pico_stdlib
    pico_multicore
    pico_time
    pico_rand
    hardware_pio
    hardware_i2c
    hardware_gpio
    ${PROJ_LIBS}
)

pico_enable_stdio_usb(pico_post_fw 1)
pico_enable_stdio_uart(pico_post_fw 0)

target_compile_definitions(pico_post_fw PRIVATE
    PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=150
    ${PROJ_DEFS}
)

# find includes
target_include_directories(pico_post_fw PUBLIC
    "${PROJECT_BINARY_DIR}"
    ${PROJ_INCS}
)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(pico_post_fw)
