# C3 workspace /bin service source list.

if(NOT DEFINED C3_BIN_DIR)
    message(FATAL_ERROR "Set C3_BIN_DIR before including c3_bin_sources.cmake")
endif()

set(C3_BIN_SRCS
    "${C3_BIN_DIR}/bin.c"
    "${C3_BIN_DIR}/bin_registry.c"
    "${C3_BIN_DIR}/bin_hardware.c"
    "${C3_BIN_DIR}/bin_hardware_adc.c"
    "${C3_BIN_DIR}/bin_hardware_i2c.c"
    "${C3_BIN_DIR}/bin_hardware_spi.c"
    "${C3_BIN_DIR}/bin_nano.c"
)

set(C3_BIN_INCLUDE_DIRS
    "${C3_BIN_DIR}"
)
