#include "editor_service.h"

#include "basic.h"
#include "input.h"
#include "storage.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define EDITOR_TEXT_MAX (64 * 1024)
#define EDITOR_READ_CHUNK 128

typedef struct {
    char path[STORAGE_PATH_MAX];
    char *text;
    size_t cap;
    size_t len;
    bool dirty;
} editor_buffer_t;

typedef struct {
    const shell_exec_io_t *io;
} editor_basic_io_t;

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

static void basic_editor_write(void *ctx, const char *text)
{
    editor_basic_io_t *basic_io = (editor_basic_io_t *)ctx;
    editor_write(basic_io ? basic_io->io : NULL, text);
}

static int basic_editor_read_line(void *ctx, char *buf, size_t len)
{
    editor_basic_io_t *basic_io = (editor_basic_io_t *)ctx;
    return editor_read_line(basic_io ? basic_io->io : NULL, buf, len);
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
    const char *basic_prefix = STORAGE_WORKSPACE_MOUNT_POINT "/basic/";

    if (!path || *skip_ws(path) == '\0') {
        return false;
    }

    if (!storage_resolve_workspace_path(cwd, path, out, out_size)) {
        return false;
    }

    if (request->mode == EDITOR_MODE_TEXT) {
        return strncmp(out, data_prefix, strlen(data_prefix)) == 0 && has_suffix_ci(out, ".txt");
    }

    if (request->mode == EDITOR_MODE_BASIC) {
        return strncmp(out, basic_prefix, strlen(basic_prefix)) == 0 && has_suffix_ci(out, ".bas");
    }

    return false;
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

static void show_help(const shell_exec_io_t *io, editor_mode_t mode)
{
    editor_write(io,
        "Commands:\r\n"
        ":w   save\r\n"
        ":q   quit if clean\r\n"
        ":q!  quit without saving\r\n"
        ":wq  save and quit\r\n"
        ":p   print buffer\r\n"
        ":clear clear buffer\r\n"
        ":help help\r\n");
    if (mode == EDITOR_MODE_BASIC) {
        editor_write(io,
            ":run run BASIC program\r\n"
            ":debug step-run BASIC program (planned)\r\n");
    }
    editor_write(io,
        "Any other line appends text.\r\n");
}

static const char *editor_mode_name(editor_mode_t mode)
{
    switch (mode) {
    case EDITOR_MODE_BASIC:
        return ".bas";
    case EDITOR_MODE_TEXT:
    default:
        return ".txt";
    }
}

static bool validate_basic_buffer(const shell_exec_io_t *io, const editor_buffer_t *buffer)
{
    size_t source_line = 1;
    const char *cursor = buffer ? buffer->text : NULL;

    while (cursor && *cursor) {
        const char *line = cursor;
        const char *end = strpbrk(line, "\r\n");
        const char *p = line;
        bool has_digits = false;
        long number = 0;

        while (end && end > line && (end[-1] == '\r' || end[-1] == '\n')) {
            end--;
        }

        while (p < (end ? end : line + strlen(line)) && isspace((unsigned char)*p)) {
            p++;
        }

        const char *limit = end ? end : line + strlen(line);
        if (p < limit) {
            while (p < limit && isdigit((unsigned char)*p)) {
                has_digits = true;
                number = (number * 10) + (*p - '0');
                p++;
            }

            if (!has_digits || number <= 0) {
                char msg[96];
                snprintf(msg, sizeof(msg), "BASIC validation failed line %u: missing line number\r\n",
                         (unsigned)source_line);
                editor_write(io, msg);
                return false;
            }

            if (p < limit && !isspace((unsigned char)*p)) {
                char msg[112];
                snprintf(msg, sizeof(msg), "BASIC validation failed line %u: malformed line number\r\n",
                         (unsigned)source_line);
                editor_write(io, msg);
                return false;
            }
        }

        if (!end) {
            break;
        }
        cursor = end;
        while (*cursor == '\r' || *cursor == '\n') {
            cursor++;
        }
        source_line++;
    }

    return true;
}

static editor_status_t validate_before_save(const shell_exec_io_t *io, const editor_request_t *request,
                                            const editor_buffer_t *buffer)
{
    if (request && request->mode == EDITOR_MODE_BASIC && !validate_basic_buffer(io, buffer)) {
        return EDITOR_STATUS_SAVE_FAILED;
    }
    return EDITOR_STATUS_OK;
}

static editor_status_t run_basic_buffer(const shell_exec_io_t *io, const editor_request_t *request,
                                        editor_buffer_t *buffer)
{
    editor_status_t status = validate_before_save(io, request, buffer);
    if (status != EDITOR_STATUS_OK) {
        return status;
    }

    status = save_buffer(buffer);
    if (status != EDITOR_STATUS_OK) {
        editor_write(io, "SAVE FAILED\r\n");
        return status;
    }
    editor_write(io, "SAVED\r\n");

    basic_program_t program;
    basic_init(&program);
    esp_err_t err = basic_load_buffer(buffer->text, buffer->len, &program);
    if (err == ESP_ERR_NO_MEM) {
        editor_write(io, "Out of memory\r\n");
        basic_clear(&program);
        return EDITOR_STATUS_OUT_OF_MEMORY;
    }
    if (err != ESP_OK) {
        editor_write(io, "BASIC load failed\r\n");
        basic_clear(&program);
        return EDITOR_STATUS_SAVE_FAILED;
    }

    editor_basic_io_t basic_ctx = {
        .io = io,
    };
    basic_io_t basic_io = {
        .read_line = basic_editor_read_line,
        .write = basic_editor_write,
        .ctx = &basic_ctx,
    };

    editor_write(io, "RUN\r\n");
    err = basic_run(&program, &basic_io);
    basic_clear(&program);
    if (err != ESP_OK) {
        return EDITOR_STATUS_SAVE_FAILED;
    }
    return EDITOR_STATUS_OK;
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
        if (request && request->mode == EDITOR_MODE_BASIC) {
            editor_write(io, "Bad editor path. Use /basic/name.bas\r\n");
        } else {
            editor_write(io, "Bad editor path. Use /data/name.txt\r\n");
        }
        free(line);
        free_buffer(buffer);
        return EDITOR_STATUS_OPEN_FAILED;
    }

    if (request->mode == EDITOR_MODE_ASM) {
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
    editor_writef(io, "%s mode. Type :help for commands.\r\n", editor_mode_name(request->mode));
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
            show_help(io, request->mode);
        } else if (strcmp(line, ":p") == 0) {
            show_buffer(io, buffer);
        } else if (strcmp(line, ":clear") == 0) {
            clear_buffer(buffer);
            editor_write(io, "CLEARED\r\n");
        } else if (strcmp(line, ":w") == 0) {
            status = validate_before_save(io, request, buffer);
            if (status != EDITOR_STATUS_OK) {
                continue;
            }
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
            status = validate_before_save(io, request, buffer);
            if (status != EDITOR_STATUS_OK) {
                continue;
            }
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
        } else if (strcmp(line, ":run") == 0) {
            if (request->mode != EDITOR_MODE_BASIC) {
                editor_write(io, "BASIC mode required\r\n");
                continue;
            }
            status = run_basic_buffer(io, request, buffer);
            if (status != EDITOR_STATUS_OK) {
                continue;
            }
        } else if (strcmp(line, ":debug") == 0) {
            editor_write(io, "BASIC debug not implemented\r\n");
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
