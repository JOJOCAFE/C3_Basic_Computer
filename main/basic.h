#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>

#define BASIC_STACK_DEPTH 8
#define BASIC_FOR_DEPTH 8

typedef struct {
    int number;
    char *text;
} basic_line_t;

typedef struct {
    basic_line_t *lines;
    size_t count;
    size_t capacity;
} basic_program_t;

typedef struct {
    size_t source_line;
    char excerpt[48];
    char reason[64];
} basic_error_t;

typedef int (*basic_read_line_fn)(void *ctx, char *buf, size_t len);
typedef void (*basic_write_fn)(void *ctx, const char *text);
typedef esp_err_t (*basic_service_exec_fn)(void *ctx, const char *command, bool hardware);

typedef struct {
    basic_read_line_fn read_line;
    basic_write_fn write;
    basic_service_exec_fn service_exec;
    void *ctx;
} basic_io_t;

void basic_init(basic_program_t *program);
bool basic_store_line(basic_program_t *program, const char *line);
void basic_clear(basic_program_t *program);
void basic_list(const basic_program_t *program, basic_write_fn write, void *ctx);
esp_err_t basic_load_buffer(const char *text, size_t len, basic_program_t *program);
esp_err_t basic_load_buffer_checked(const char *text, size_t len, basic_program_t *program,
                                    basic_error_t *error);
esp_err_t basic_load_file(const char *path, basic_program_t *program);
esp_err_t basic_save_file(const char *path, const basic_program_t *program);
esp_err_t basic_run(const basic_program_t *program, const basic_io_t *io);
esp_err_t basic_debug(const basic_program_t *program, const basic_io_t *io);
esp_err_t basic_execute_immediate(basic_program_t *program, const char *line, const basic_io_t *io);
