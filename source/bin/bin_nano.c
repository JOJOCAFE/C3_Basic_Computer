#include "bin_internal.h"

#include "editor_service.h"

#include <ctype.h>
#include <string.h>
#include <strings.h>

static const char *skip_ws_local(const char *s)
{
    while (s && *s && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

static char *next_token_local(char **cursor)
{
    char *start = (char *)skip_ws_local(*cursor);
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

shell_status_t bin_nano_exec(const char *args, const shell_exec_io_t *io)
{
    char buf[160];
    char *cursor = buf;
    char *path;
    editor_request_t request;

    if (!args) {
        return SHELL_STATUS_BAD_INPUT;
    }

    strncpy(buf, args, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    path = next_token_local(&cursor);
    if (!path) {
        bin_write(io, "Usage: EDIT /data/name.txt\r\n");
        return SHELL_STATUS_BAD_INPUT;
    }

    if (*skip_ws_local(cursor) != '\0') {
        bin_write(io, "Bad input\r\n");
        return SHELL_STATUS_BAD_INPUT;
    }

    request.path = path;
    request.cwd = "/";
    request.mode = EDITOR_MODE_TEXT;

    return editor_status_to_shell_status(editor_run(&request, io));
}
