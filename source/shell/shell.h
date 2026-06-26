#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
    SHELL_STATUS_OK = 0,
    SHELL_STATUS_EMPTY,
    SHELL_STATUS_UNKNOWN_COMMAND,
    SHELL_STATUS_BAD_INPUT,
    SHELL_STATUS_BAD_PATH,
    SHELL_STATUS_NOT_FOUND,
    SHELL_STATUS_EXISTS,
    SHELL_STATUS_IS_DIRECTORY,
    SHELL_STATUS_NOT_DIRECTORY,
    SHELL_STATUS_IO_ERROR,
    SHELL_STATUS_CANCELLED,
    SHELL_STATUS_NOT_ALLOWED,
} shell_status_t;

typedef enum {
    SHELL_COMMAND_HELP = 0,
    SHELL_COMMAND_PWD,
    SHELL_COMMAND_LS,
    SHELL_COMMAND_CD,
    SHELL_COMMAND_MKDIR,
    SHELL_COMMAND_RMDIR,
    SHELL_COMMAND_CAT,
    SHELL_COMMAND_WRITE,
    SHELL_COMMAND_RM,
    SHELL_COMMAND_CP,
    SHELL_COMMAND_MV,
    SHELL_COMMAND_RENEW,
} shell_command_t;

typedef void (*shell_write_fn)(void *ctx, const void *data, size_t len);
typedef int (*shell_read_line_fn)(void *ctx, char *buf, size_t len);
typedef struct shell_exec_io shell_exec_io_t;
typedef shell_status_t (*shell_external_exec_fn)(const char *line, const shell_exec_io_t *io);

enum {
    SHELL_EXEC_ALLOW_INTERACTIVE = 1u << 0,
    SHELL_EXEC_ALLOW_DESTRUCTIVE = 1u << 1,
};

struct shell_exec_io {
    shell_write_fn write;
    shell_read_line_fn read_line;
    void *ctx;
    uint32_t flags;
    const char *stdin_text;
    size_t stdin_len;
};

void shell_reset(void);
void shell_register_external_exec(shell_external_exec_fn fn);
void shell_run(void);

shell_status_t shell_exec_line(const char *line, const shell_exec_io_t *io);
shell_status_t shell_exec_command(shell_command_t command, const char *args, const shell_exec_io_t *io);
shell_status_t shell_command_from_name(const char *name, shell_command_t *out);
const char *shell_status_name(shell_status_t status);
//Keep Going.
