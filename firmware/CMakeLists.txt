# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(PICO_DEOPTIMIZED_DEBUG 1)
endif()

include(pico_sdk_import.cmake)

project(pico_post_fw
    VERSION 0.5.0
    LANGUAGES C CXX ASM
)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# # USER CONFIGURABLE PARAMS
set(CALIBADJ_5V 1.030)
set(CALIBADJ_12V 1.017)

list(APPEND PROJ_DEFS PICOPOST_STANDBY_TIMER=15)

option(PICOPOST_USB_FALLBACK "Enable serial output if display not found" OFF)
option(PICOPOST_SUPPORT_REV5 "[EXPERIMENTAL] Enable support for older Rev5 PCB" OFF)

if(PICOPOST_USB_FALLBACK)
    list(APPEND PROJ_DEFS PICOPOST_USB_FALLBACK)
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

set(PICOPOST_UF2_FILENAME "PicoPOST-v${CMAKE_PROJECT_VERSION}")

target_compile_definitions(pico_post_fw PRIVATE
    REQ_CLOCK_KHZ=300000
    QUEUE_DEPTH=8192
    PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=150
    PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64
    PICO_FLASH_SPI_CLKDIV=4
    ${PROJ_DEFS}
)

# find includes
target_include_directories(pico_post_fw PUBLIC
    "${PROJECT_BINARY_DIR}"
    ${PROJ_INCS}
)

# create map/bin/hex/uf2 file in addition to ELF.
set_target_properties(pico_post_fw PROPERTIES OUTPUT_NAME "${PICOPOST_UF2_FILENAME}")
pico_add_extra_outputs(pico_post_fw)
