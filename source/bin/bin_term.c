#include "bin_internal.h"

#include "terminal.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define BIN_TERM_BUF 128

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

static int term_write(void *ctx, const char *data, size_t len)
{
    const shell_exec_io_t *io = (const shell_exec_io_t *)ctx;
    if (!data || len == 0) {
        return 0;
    }
    if (io && io->write) {
        io->write(io->ctx, data, len);
        return (int)len;
    }
    bin_write(io, data);
    return (int)len;
}

static bool parse_uint(const char *text, unsigned *out)
{
    if (!text || !out || *text == '\0') {
        return false;
    }
    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > 65535UL) {
        return false;
    }
    *out = (unsigned)value;
    return true;
}

static shell_status_t term_usage(const shell_exec_io_t *io)
{
    bin_write(io,
              "Usage: /bin/term clear|home|goto -r <row> -c <col>|"
              "color -f <0-7> [-b <0-7>]|reset|hide-cursor|show-cursor\r\n");
    return SHELL_STATUS_BAD_INPUT;
}

static shell_status_t term_bad_argument(const shell_exec_io_t *io)
{
    bin_write(io, "Bad argument\r\n");
    return SHELL_STATUS_BAD_INPUT;
}

static bool no_more_args(char *cursor)
{
    return *skip_ws(cursor) == '\0';
}

static shell_status_t run_goto(char *cursor, terminal_t *term, const shell_exec_io_t *io)
{
    unsigned row = 0;
    unsigned col = 0;
    bool have_row = false;
    bool have_col = false;

    char *token = NULL;
    while ((token = next_token(&cursor)) != NULL) {
        char *value = next_token(&cursor);
        unsigned parsed = 0;
        if (!value || !parse_uint(value, &parsed)) {
            return term_bad_argument(io);
        }
        if (strcasecmp(token, "-r") == 0 && !have_row) {
            row = parsed;
            have_row = true;
        } else if (strcasecmp(token, "-c") == 0 && !have_col) {
            col = parsed;
            have_col = true;
        } else {
            return term_bad_argument(io);
        }
    }

    if (!have_row || !have_col || terminal_goto(term, row, col) != 0) {
        return term_bad_argument(io);
    }
    return SHELL_STATUS_OK;
}

static shell_status_t run_color(char *cursor, terminal_t *term, const shell_exec_io_t *io)
{
    unsigned fg = 0;
    unsigned bg = 0;
    bool have_fg = false;
    bool have_bg = false;

    char *token = NULL;
    while ((token = next_token(&cursor)) != NULL) {
        char *value = next_token(&cursor);
        unsigned parsed = 0;
        if (!value || !parse_uint(value, &parsed) || parsed > 7) {
            return term_bad_argument(io);
        }
        if (strcasecmp(token, "-f") == 0 && !have_fg) {
            fg = parsed;
            have_fg = true;
        } else if (strcasecmp(token, "-b") == 0 && !have_bg) {
            bg = parsed;
            have_bg = true;
        } else {
            return term_bad_argument(io);
        }
    }

    if (!have_fg || terminal_color(term, fg, bg, have_bg) != 0) {
        return term_bad_argument(io);
    }
    return SHELL_STATUS_OK;
}

shell_status_t bin_term_exec(const char *args, const shell_exec_io_t *io)
{
    char buf[BIN_TERM_BUF];
    const char *trimmed = skip_ws(args);
    if (!trimmed || *trimmed == '\0') {
        return term_usage(io);
    }
    if (strlen(trimmed) >= sizeof(buf)) {
        return term_bad_argument(io);
    }
    strncpy(buf, trimmed, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *cursor = buf;
    char *command = next_token(&cursor);
    if (!command) {
        return term_usage(io);
    }

    terminal_t term = {
        .write = term_write,
        .ctx = (void *)io,
    };

    if (strcasecmp(command, "clear") == 0) {
        if (!no_more_args(cursor) || terminal_clear(&term) != 0) {
            return term_bad_argument(io);
        }
        return SHELL_STATUS_OK;
    }
    if (strcasecmp(command, "home") == 0) {
        if (!no_more_args(cursor) || terminal_home(&term) != 0) {
            return term_bad_argument(io);
        }
        return SHELL_STATUS_OK;
    }
    if (strcasecmp(command, "goto") == 0) {
        return run_goto(cursor, &term, io);
    }
    if (strcasecmp(command, "color") == 0) {
        return run_color(cursor, &term, io);
    }
    if (strcasecmp(command, "reset") == 0) {
        if (!no_more_args(cursor) || terminal_reset(&term) != 0) {
            return term_bad_argument(io);
        }
        return SHELL_STATUS_OK;
    }
    if (strcasecmp(command, "hide-cursor") == 0) {
        if (!no_more_args(cursor) || terminal_hide_cursor(&term) != 0) {
            return term_bad_argument(io);
        }
        return SHELL_STATUS_OK;
    }
    if (strcasecmp(command, "show-cursor") == 0) {
        if (!no_more_args(cursor) || terminal_show_cursor(&term) != 0) {
            return term_bad_argument(io);
        }
        return SHELL_STATUS_OK;
    }

    return term_usage(io);
}
//Keep Going.
