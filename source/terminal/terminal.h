#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*terminal_write_fn)(void *ctx, const char *data, size_t len);

typedef struct {
    terminal_write_fn write;
    void *ctx;
} terminal_t;

int terminal_clear(terminal_t *terminal);
int terminal_home(terminal_t *terminal);
int terminal_goto(terminal_t *terminal, unsigned row, unsigned col);
int terminal_color(terminal_t *terminal, unsigned fg, unsigned bg, bool has_bg);
int terminal_reset(terminal_t *terminal);
int terminal_hide_cursor(terminal_t *terminal);
int terminal_show_cursor(terminal_t *terminal);

#ifdef __cplusplus
}
#endif
//Keep Going.
