set(C3_SHELL_DIR "${CMAKE_CURRENT_LIST_DIR}/../source/shell")
include("${C3_SHELL_DIR}/c3_shell_sources.cmake")
set(C3_HARDWARE_DIR "${CMAKE_CURRENT_LIST_DIR}/../source/hardware")
include("${C3_HARDWARE_DIR}/c3_hardware_sources.cmake")
set(C3_BIN_DIR "${CMAKE_CURRENT_LIST_DIR}/../source/bin")
include("${C3_BIN_DIR}/c3_bin_sources.cmake")
set(C3_EDITOR_DIR "${CMAKE_CURRENT_LIST_DIR}/../source/editor")
include("${C3_EDITOR_DIR}/c3_editor_sources.cmake")

idf_component_register(
    SRCS
        "main.c"
        ${C3_SHELL_SRCS}
        ${C3_HARDWARE_SRCS}
        ${C3_BIN_SRCS}
        ${C3_EDITOR_SRCS}
        "input_serial.c"
        "input_ble_hid.c"
        "basic.c"
        "basic_hardware.c"
        "storage.c"
    INCLUDE_DIRS
        "."
        ${C3_SHELL_INCLUDE_DIRS}
        ${C3_HARDWARE_INCLUDE_DIRS}
        ${C3_BIN_INCLUDE_DIRS}
        ${C3_EDITOR_INCLUDE_DIRS}
    REQUIRES
        ${C3_HARDWARE_REQUIRES}
        esp_driver_usb_serial_jtag
        esp_partition
        joltwallet__littlefs
)
