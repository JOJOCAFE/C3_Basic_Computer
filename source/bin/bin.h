#pragma once

#include "shell.h"

#ifdef __cplusplus
extern "C" {
#endif

shell_status_t bin_exec_line(const char *line, const shell_exec_io_t *io);

#ifdef __cplusplus
}
#endif
