#pragma once

#include <stddef.h>

#include "shell.h"

void bin_write(const shell_exec_io_t *io, const char *text);
void bin_writef(const shell_exec_io_t *io, const char *fmt, ...);
shell_status_t bin_hardware_exec(const char *args, const shell_exec_io_t *io);
shell_status_t bin_hardware_adc_exec(const char *args, const shell_exec_io_t *io);
shell_status_t bin_hardware_i2c_exec(const char *args, const shell_exec_io_t *io);
shell_status_t bin_hardware_spi_exec(const char *args, const shell_exec_io_t *io);
shell_status_t bin_nano_exec(const char *args, const shell_exec_io_t *io);
