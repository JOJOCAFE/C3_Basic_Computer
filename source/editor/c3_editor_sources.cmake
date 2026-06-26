# C3 editor service source list.

if(NOT DEFINED C3_EDITOR_DIR)
    message(FATAL_ERROR "Set C3_EDITOR_DIR before including c3_editor_sources.cmake")
endif()

set(C3_EDITOR_SRCS
    "${C3_EDITOR_DIR}/editor_service.c"
)

set(C3_EDITOR_INCLUDE_DIRS
    "${C3_EDITOR_DIR}"
)
