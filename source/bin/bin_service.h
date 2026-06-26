#pragma once

#include "shell.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef shell_status_t (*bin_service_exec_fn)(const char *args, const shell_exec_io_t *io);
typedef int (*bin_stream_read_fn)(void *ctx, char *buf, size_t len);
typedef int (*bin_stream_write_fn)(void *ctx, const char *buf, size_t len);

typedef struct {
    bin_stream_read_fn read;
    bin_stream_write_fn write;
    void *ctx;
} bin_stream_t;

typedef struct {
    bin_stream_t in;
    bin_stream_t out;
    bin_stream_t err;
} bin_service_io_t;

typedef struct {
    const char *name;
    const char *const *aliases;
    size_t alias_count;
    bin_service_exec_fn exec;
    bool removable;
} bin_service_t;

const bin_service_t *bin_find_service(const char *name);
void bin_list_services(const shell_exec_io_t *io);

#ifdef __cplusplus
}
#endif
//Keep Going.
