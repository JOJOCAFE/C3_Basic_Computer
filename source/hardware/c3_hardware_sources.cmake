# C3 hardware service source list for ESP-IDF projects.
#
# Include this file from an ESP-IDF component CMakeLists.txt after setting
# C3_HARDWARE_DIR to the absolute or component-relative source/hardware path.
#
# Example:
#   set(C3_HARDWARE_DIR "${CMAKE_CURRENT_LIST_DIR}/../source/hardware")
#   include("${C3_HARDWARE_DIR}/c3_hardware_sources.cmake")
#   idf_component_register(
#       SRCS "main.c" ${C3_HARDWARE_SRCS}
#       INCLUDE_DIRS "." ${C3_HARDWARE_INCLUDE_DIRS}
#       REQUIRES ${C3_HARDWARE_REQUIRES}
#   )

if(NOT DEFINED C3_HARDWARE_DIR)
    message(FATAL_ERROR "Set C3_HARDWARE_DIR before including c3_hardware_sources.cmake")
endif()

set(C3_HARDWARE_SRCS
    "${C3_HARDWARE_DIR}/hardware.c"
    "${C3_HARDWARE_DIR}/hardware_adc.c"
    "${C3_HARDWARE_DIR}/hardware_gpio.c"
    "${C3_HARDWARE_DIR}/hardware_i2c.c"
    "${C3_HARDWARE_DIR}/hardware_spi.c"
)

set(C3_HARDWARE_INCLUDE_DIRS
    "${C3_HARDWARE_DIR}"
)

set(C3_HARDWARE_REQUIRES
    esp_adc
    esp_driver_gpio
    esp_driver_i2c
    esp_driver_spi
)
