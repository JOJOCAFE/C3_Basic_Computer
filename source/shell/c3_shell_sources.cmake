# C3 shell source list for ESP-IDF projects.
#
# Include this file from an ESP-IDF component CMakeLists.txt after setting
# C3_SHELL_DIR to the absolute or component-relative source/shell path.
#
# Example:
#   set(C3_SHELL_DIR "${CMAKE_CURRENT_LIST_DIR}/../source/shell")
#   include("${C3_SHELL_DIR}/c3_shell_sources.cmake")
#   idf_component_register(
#       SRCS "main.c" ${C3_SHELL_SRCS}
#       INCLUDE_DIRS "." ${C3_SHELL_INCLUDE_DIRS} ${C3_SHELL_REFERENCE_PORT_INCLUDE_DIRS}
#   )
#
# The reference port headers define input.h and storage.h. A real firmware must
# still provide matching .c implementations for those APIs.

if(NOT DEFINED C3_SHELL_DIR)
    message(FATAL_ERROR "Set C3_SHELL_DIR before including c3_shell_sources.cmake")
endif()

set(C3_SHELL_SRCS
    "${C3_SHELL_DIR}/shell.c"
)

set(C3_SHELL_INCLUDE_DIRS
    "${C3_SHELL_DIR}"
)

set(C3_SHELL_REFERENCE_PORT_INCLUDE_DIRS
    "${C3_SHELL_DIR}/port"
)
