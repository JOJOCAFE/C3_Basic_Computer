#include "bin.h"

#include "bin_internal.h"
#include "bin_service.h"
#include "input.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define BIN_BUF 256

static const char *skip_ws(const char *s)
{
    while (s && *s && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

static char *next_token(char **cursor)
{
    char *start = (char *)skip_ws(*cursor);
    if (!start || *start == '\0') {
        *cursor = start;
        return NULL;
    }

    char *end = start;
    while (*end && !isspace((unsigned char)*end)) {
        end++;
    }
    if (*end) {
        *end++ = '\0';
    }
    *cursor = end;
    return start;
}

void bin_write(const shell_exec_io_t *io, const char *text)
{
    if (!text) {
        return;
    }

    size_t len = strlen(text);
    if (io && io->write) {
        io->write(io->ctx, text, len);
        return;
    }
    input_write_bytes(text, len);
}

void bin_writef(const shell_exec_io_t *io, const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    bin_write(io, buf);
}

shell_status_t bin_exec_line(const char *line, const shell_exec_io_t *io)
{
    if (!line) {
        return SHELL_STATUS_BAD_INPUT;
    }

    char buf[BIN_BUF];
    const char *trimmed = skip_ws(line);
    strncpy(buf, trimmed, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *cursor = buf;
    char *command = next_token(&cursor);
    if (!command) {
        return SHELL_STATUS_EMPTY;
    }

    if (strcasecmp(command, "/bin") == 0 || strcasecmp(command, "bin") == 0) {
        char *subcommand = next_token(&cursor);
        if (subcommand && strcasecmp(subcommand, "list") == 0 && *skip_ws(cursor) == '\0') {
            bin_list_services(io);
            return SHELL_STATUS_OK;
        }
        bin_write(io, "Usage: /bin list\r\n");
        return SHELL_STATUS_BAD_INPUT;
    }

    const bin_service_t *service = bin_find_service(command);
    if (service) {
        return service->exec(cursor, io);
    }

    return SHELL_STATUS_UNKNOWN_COMMAND;
}
