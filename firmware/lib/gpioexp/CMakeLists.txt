add_library(gpio_exp gpioexp.cpp)

target_link_libraries(gpio_exp hardware_i2c pico_stdlib)

target_include_directories(gpio_exp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(gpio_exp PUBLIC "${PROJECT_BINARY_DIR}/cfg")
