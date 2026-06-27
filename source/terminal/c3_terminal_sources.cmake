# C3 terminal helper source list.

if(NOT DEFINED C3_TERMINAL_DIR)
    message(FATAL_ERROR "Set C3_TERMINAL_DIR before including c3_terminal_sources.cmake")
endif()

set(C3_TERMINAL_SRCS
    "${C3_TERMINAL_DIR}/terminal.c"
)

set(C3_TERMINAL_INCLUDE_DIRS
    "${C3_TERMINAL_DIR}"
)
