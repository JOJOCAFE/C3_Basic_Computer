#pragma once

#include "shell.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EDITOR_STATUS_OK = 0,
    EDITOR_STATUS_CANCELLED = 1,
    EDITOR_STATUS_OPEN_FAILED = 2,
    EDITOR_STATUS_SAVE_FAILED = 3,
    EDITOR_STATUS_OUT_OF_MEMORY = 4,
    EDITOR_STATUS_TOO_LARGE = 5,
} editor_status_t;

typedef enum {
    EDITOR_MODE_TEXT = 0,
    EDITOR_MODE_BASIC,
    EDITOR_MODE_ASM,
} editor_mode_t;

typedef struct {
    const char *path;
    const char *cwd;
    editor_mode_t mode;
} editor_request_t;

editor_status_t editor_run(const editor_request_t *request, const shell_exec_io_t *io);
shell_status_t editor_status_to_shell_status(editor_status_t status);

#ifdef __cplusplus
}
#endif
