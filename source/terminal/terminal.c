#include "terminal.h"

#include <stdio.h>
#include <string.h>

#define TERMINAL_ESC "\x1B"

static int terminal_write_all(terminal_t *terminal, const char *data)
{
    if (terminal == NULL || terminal->write == NULL || data == NULL) {
        return -1;
    }

    const size_t len = strlen(data);
    if (len == 0) {
        return 0;
    }

    return terminal->write(terminal->ctx, data, len) < 0 ? -1 : 0;
}

int terminal_clear(terminal_t *terminal)
{
    return terminal_write_all(terminal, TERMINAL_ESC "[2J" TERMINAL_ESC "[H");
}

int terminal_home(terminal_t *terminal)
{
    return terminal_write_all(terminal, TERMINAL_ESC "[H");
}

int terminal_goto(terminal_t *terminal, unsigned row, unsigned col)
{
    if (row == 0 || col == 0) {
        return -1;
    }

    char seq[32];
    const int written = snprintf(seq, sizeof(seq), TERMINAL_ESC "[%u;%uH", row, col);
    if (written < 0 || (size_t)written >= sizeof(seq)) {
        return -1;
    }

    return terminal_write_all(terminal, seq);
}

int terminal_color(terminal_t *terminal, unsigned fg, unsigned bg, bool has_bg)
{
    if (fg > 7 || (has_bg && bg > 7)) {
        return -1;
    }

    char seq[16];
    const int written = has_bg
        ? snprintf(seq, sizeof(seq), TERMINAL_ESC "[3%u;4%um", fg, bg)
        : snprintf(seq, sizeof(seq), TERMINAL_ESC "[3%um", fg);
    if (written < 0 || (size_t)written >= sizeof(seq)) {
        return -1;
    }

    return terminal_write_all(terminal, seq);
}

int terminal_reset(terminal_t *terminal)
{
    return terminal_write_all(terminal, TERMINAL_ESC "[0m");
}

int terminal_hide_cursor(terminal_t *terminal)
{
    return terminal_write_all(terminal, TERMINAL_ESC "[?25l");
}

int terminal_show_cursor(terminal_t *terminal)
{
    return terminal_write_all(terminal, TERMINAL_ESC "[?25h");
}
//Keep Going.
