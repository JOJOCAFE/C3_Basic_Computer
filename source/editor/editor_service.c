#include "editor_service.h"

#include "input.h"
#include "storage.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define EDITOR_TEXT_MAX (16 * 1024)
#define EDITOR_READ_CHUNK 128

typedef struct {
    char path[STORAGE_PATH_MAX];
    char *text;
    size_t cap;
    size_t len;
    bool dirty;
} editor_buffer_t;

static void free_buffer(editor_buffer_t *buffer)
{
    if (!buffer) {
        return;
    }
    free(buffer->text);
    free(buffer);
}

static void editor_write(const shell_exec_io_t *io, const char *text)
{
    if (!text) {
        return;
    }
    if (io && io->write) {
        io->write(io->ctx, text, strlen(text));
        return;
    }
    input_write_bytes(text, strlen(text));
}

static void editor_writef(const shell_exec_io_t *io, const char *fmt, const char *value)
{
    char buf[224];
    snprintf(buf, sizeof(buf), fmt, value ? value : "");
    editor_write(io, buf);
}

static int editor_read_line(const shell_exec_io_t *io, char *buf, size_t len)
{
    if (!buf || len == 0) {
        return -1;
    }
    if (!io || !io->read_line) {
        return input_read_line(buf, len);
    }
    return io->read_line(io->ctx, buf, len);
}

static const char *skip_ws(const char *s)
{
    while (s && *s && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

static bool has_suffix_ci(const char *text, const char *suffix)
{
    size_t text_len;
    size_t suffix_len;

    if (!text || !suffix) {
        return false;
    }

    text_len = strlen(text);
    suffix_len = strlen(suffix);
    if (suffix_len > text_len) {
        return false;
    }

    return strcasecmp(text + text_len - suffix_len, suffix) == 0;
}

static bool resolve_editor_path(const editor_request_t *request, char *out, size_t out_size)
{
    const char *cwd = (request && request->cwd) ? request->cwd : "/";
    const char *path = request ? request->path : NULL;
    const char *data_prefix = STORAGE_WORKSPACE_MOUNT_POINT "/data/";

    if (!path || *skip_ws(path) == '\0') {
        return false;
    }

    if (!storage_resolve_workspace_path(cwd, path, out, out_size)) {
        return false;
    }

    return strncmp(out, data_prefix, strlen(data_prefix)) == 0 && has_suffix_ci(out, ".txt");
}

static editor_status_t load_buffer(editor_buffer_t *buffer)
{
    FILE *f;
    struct stat st;

    buffer->text = calloc(1, EDITOR_TEXT_MAX);
    if (!buffer->text) {
        return EDITOR_STATUS_OUT_OF_MEMORY;
    }
    buffer->cap = EDITOR_TEXT_MAX;
    buffer->text[0] = '\0';
    buffer->len = 0;
    buffer->dirty = false;

    if (stat(buffer->path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return EDITOR_STATUS_OPEN_FAILED;
    }

    f = fopen(buffer->path, "r");
    if (!f) {
        return EDITOR_STATUS_OK;
    }

    while (buffer->len < buffer->cap - 1) {
        size_t remaining = buffer->cap - 1 - buffer->len;
        size_t chunk = remaining < EDITOR_READ_CHUNK ? remaining : EDITOR_READ_CHUNK;
        size_t n = fread(buffer->text + buffer->len, 1, chunk, f);
        buffer->len += n;
        if (n < chunk) {
            break;
        }
    }

    if (ferror(f)) {
        fclose(f);
        return EDITOR_STATUS_OPEN_FAILED;
    }

    if (buffer->len == buffer->cap - 1) {
        int extra = fgetc(f);
        if (extra != EOF) {
            fclose(f);
            return EDITOR_STATUS_TOO_LARGE;
        }
        if (ferror(f)) {
            fclose(f);
            return EDITOR_STATUS_OPEN_FAILED;
        }
    }

    fclose(f);
    buffer->text[buffer->len] = '\0';
    return EDITOR_STATUS_OK;
}

static editor_status_t save_buffer(editor_buffer_t *buffer)
{
    FILE *f = fopen(buffer->path, "w");
    bool ok;

    if (!f) {
        return EDITOR_STATUS_SAVE_FAILED;
    }

    ok = fwrite(buffer->text, 1, buffer->len, f) == buffer->len;
    if (fclose(f) != 0) {
        ok = false;
    }

    if (!ok) {
        return EDITOR_STATUS_SAVE_FAILED;
    }

    buffer->dirty = false;
    return EDITOR_STATUS_OK;
}

static bool append_line(editor_buffer_t *buffer, const char *line)
{
    size_t line_len = strlen(line);
    bool add_newline = buffer->len > 0;
    size_t need = line_len + (add_newline ? 1u : 0u);

    if (buffer->len + need >= buffer->cap) {
        return false;
    }

    if (add_newline) {
        buffer->text[buffer->len++] = '\n';
    }
    memcpy(buffer->text + buffer->len, line, line_len);
    buffer->len += line_len;
    buffer->text[buffer->len] = '\0';
    buffer->dirty = true;
    return true;
}

static void clear_buffer(editor_buffer_t *buffer)
{
    buffer->text[0] = '\0';
    buffer->len = 0;
    buffer->dirty = true;
}

static void show_buffer(const shell_exec_io_t *io, const editor_buffer_t *buffer)
{
    editor_write(io, "\r\n--- FILE ---\r\n");
    if (buffer->len > 0) {
        editor_write(io, buffer->text);
        if (buffer->text[buffer->len - 1] != '\n' && buffer->text[buffer->len - 1] != '\r') {
            editor_write(io, "\r\n");
        }
    }
    editor_write(io, "--- END ---\r\n");
}

static void show_help(const shell_exec_io_t *io)
{
    editor_write(io,
        "Commands:\r\n"
        ":w   save\r\n"
        ":q   quit if clean\r\n"
        ":q!  quit without saving\r\n"
        ":wq  save and quit\r\n"
        ":p   print buffer\r\n"
        ":clear clear buffer\r\n"
        ":help help\r\n"
        "Any other line appends text.\r\n");
}

editor_status_t editor_run(const editor_request_t *request, const shell_exec_io_t *io)
{
    editor_buffer_t *buffer;
    editor_status_t status;
    char *line;

    buffer = calloc(1, sizeof(*buffer));
    if (!buffer) {
        editor_write(io, "Out of memory\r\n");
        return EDITOR_STATUS_OUT_OF_MEMORY;
    }

    line = calloc(1, EDITOR_TEXT_MAX);
    if (!line) {
        editor_write(io, "Out of memory\r\n");
        free_buffer(buffer);
        return EDITOR_STATUS_OUT_OF_MEMORY;
    }

    if (!resolve_editor_path(request, buffer->path, sizeof(buffer->path))) {
        editor_write(io, "Bad editor path. Use /data/name.txt\r\n");
        free(line);
        free_buffer(buffer);
        return EDITOR_STATUS_OPEN_FAILED;
    }

    if (request->mode != EDITOR_MODE_TEXT) {
        editor_write(io, "Language plugin not available\r\n");
        free(line);
        free_buffer(buffer);
        return EDITOR_STATUS_OPEN_FAILED;
    }

    status = load_buffer(buffer);
    if (status != EDITOR_STATUS_OK) {
        editor_write(io, status == EDITOR_STATUS_TOO_LARGE ? "File too large\r\n" :
                         status == EDITOR_STATUS_OUT_OF_MEMORY ? "Out of memory\r\n" :
                         "Open failed\r\n");
        free(line);
        free_buffer(buffer);
        return status;
    }

    editor_writef(io, "EDIT %s\r\n", request->path);
    editor_write(io, ".txt mode. Type :help for commands.\r\n");
    show_buffer(io, buffer);

    while (true) {
        editor_write(io, buffer->dirty ? "edit* " : "edit> ");
        if (editor_read_line(io, line, EDITOR_TEXT_MAX) <= 0) {
            editor_write(io, "Input ended\r\n");
            status = buffer->dirty ? EDITOR_STATUS_CANCELLED : EDITOR_STATUS_OK;
            free(line);
            free_buffer(buffer);
            return status;
        }

        if (strcmp(line, ":help") == 0) {
            show_help(io);
        } else if (strcmp(line, ":p") == 0) {
            show_buffer(io, buffer);
        } else if (strcmp(line, ":clear") == 0) {
            clear_buffer(buffer);
            editor_write(io, "CLEARED\r\n");
        } else if (strcmp(line, ":w") == 0) {
            status = save_buffer(buffer);
            if (status != EDITOR_STATUS_OK) {
                editor_write(io, "SAVE FAILED\r\n");
                free(line);
                free_buffer(buffer);
                return status;
            }
            editor_write(io, "SAVED\r\n");
        } else if (strcmp(line, ":q") == 0) {
            if (buffer->dirty) {
                editor_write(io, "Unsaved changes. Use :wq or :q!\r\n");
                continue;
            }
            free(line);
            free_buffer(buffer);
            return EDITOR_STATUS_OK;
        } else if (strcmp(line, ":q!") == 0) {
            free(line);
            free_buffer(buffer);
            return EDITOR_STATUS_CANCELLED;
        } else if (strcmp(line, ":wq") == 0) {
            status = save_buffer(buffer);
            if (status != EDITOR_STATUS_OK) {
                editor_write(io, "SAVE FAILED\r\n");
                free(line);
                free_buffer(buffer);
                return status;
            }
            editor_write(io, "SAVED\r\n");
            free(line);
            free_buffer(buffer);
            return EDITOR_STATUS_OK;
        } else if (line[0] == ':') {
            editor_write(io, "Unknown editor command\r\n");
        } else if (!append_line(buffer, line)) {
            editor_write(io, "Buffer full\r\n");
            free(line);
            free_buffer(buffer);
            return EDITOR_STATUS_SAVE_FAILED;
        }
    }
}

shell_status_t editor_status_to_shell_status(editor_status_t status)
{
    switch (status) {
    case EDITOR_STATUS_OK:
        return SHELL_STATUS_OK;
    case EDITOR_STATUS_CANCELLED:
        return SHELL_STATUS_CANCELLED;
    case EDITOR_STATUS_OPEN_FAILED:
        return SHELL_STATUS_BAD_PATH;
    case EDITOR_STATUS_SAVE_FAILED:
        return SHELL_STATUS_IO_ERROR;
    case EDITOR_STATUS_OUT_OF_MEMORY:
        return SHELL_STATUS_IO_ERROR;
    case EDITOR_STATUS_TOO_LARGE:
        return SHELL_STATUS_IO_ERROR;
    default:
        return SHELL_STATUS_IO_ERROR;
    }
}
