#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>

#define BASIC_MAX_LINES 64
#define BASIC_LINE_TEXT 128
#define BASIC_STACK_DEPTH 8
#define BASIC_FOR_DEPTH 8

typedef struct {
    int number;
    char text[BASIC_LINE_TEXT];
} basic_line_t;

typedef struct {
    basic_line_t lines[BASIC_MAX_LINES];
    size_t count;
} basic_program_t;

typedef int (*basic_read_line_fn)(void *ctx, char *buf, size_t len);
typedef void (*basic_write_fn)(void *ctx, const char *text);

typedef struct {
    basic_read_line_fn read_line;
    basic_write_fn write;
    void *ctx;
} basic_io_t;

void basic_init(basic_program_t *program);
bool basic_store_line(basic_program_t *program, const char *line);
void basic_clear(basic_program_t *program);
void basic_list(const basic_program_t *program, basic_write_fn write, void *ctx);
esp_err_t basic_load_file(const char *path, basic_program_t *program);
esp_err_t basic_save_file(const char *path, const basic_program_t *program);
esp_err_t basic_run(const basic_program_t *program, const basic_io_t *io);
esp_err_t basic_execute_immediate(basic_program_t *program, const char *line, const basic_io_t *io);
