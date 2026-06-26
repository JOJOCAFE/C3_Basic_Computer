set(C3_SHELL_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
include("${C3_SHELL_DIR}/c3_shell_sources.cmake")

idf_component_register(
    SRCS
        "app_main.c"
        "input_serial.c"
        "storage_fatfs.c"
        ${C3_SHELL_SRCS}
    INCLUDE_DIRS
        "."
        ${C3_SHELL_INCLUDE_DIRS}
        ${C3_SHELL_REFERENCE_PORT_INCLUDE_DIRS}
    REQUIRES
        driver
        fatfs
        wear_levelling
)
