#include "shell.h"

#include "input.h"
#include "storage.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define SHELL_BUF 256
#define FILE_IO_BUF 128
#define SHELL_PIPE_BUF 2048

static char g_workspace_cwd[STORAGE_PATH_MAX];
static const shell_exec_io_t *g_exec_io;
static shell_status_t g_exec_status;
static shell_external_exec_fn g_external_exec;

static void shell_puts(const char *text);
static void shell_write_bytes(const void *data, size_t len);
static shell_status_t shell_exec_line_no_pipe(const char *line, const shell_exec_io_t *io);

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

static bool resolve_workspace_arg(const char *arg, char *out, size_t out_size)
{
    if (!arg || *skip_ws(arg) == '\0') {
        return false;
    }
    return storage_resolve_workspace_path(g_workspace_cwd, arg, out, out_size);
}

static bool path_is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool workspace_child_path(const char *path)
{
    const char *root = STORAGE_WORKSPACE_MOUNT_POINT;
    size_t root_len = strlen(root);

    return path &&
           strncmp(path, root, root_len) == 0 &&
           path[root_len] == '/' &&
           path[root_len + 1] != '\0';
}

static bool path_is_inside(const char *parent, const char *child)
{
    size_t parent_len = strlen(parent);
    return child &&
           strncmp(parent, child, parent_len) == 0 &&
           child[parent_len] == '/';
}

static void shell_set_status(shell_status_t status)
{
    if (g_exec_status == SHELL_STATUS_OK || status != SHELL_STATUS_OK) {
        g_exec_status = status;
    }
}

static void shell_error(const char *text, shell_status_t status)
{
    shell_set_status(status);
    shell_puts(text);
    shell_puts("\r\n");
}

static void shell_ok(void)
{
    shell_set_status(SHELL_STATUS_OK);
    shell_puts("OK\r\n");
}

static uint32_t shell_exec_flags(void)
{
    return g_exec_io ? g_exec_io->flags : 0;
}

static const char *shell_stdin_text(void)
{
    return g_exec_io ? g_exec_io->stdin_text : NULL;
}

static size_t shell_stdin_len(void)
{
    return g_exec_io ? g_exec_io->stdin_len : 0;
}

static void shell_print_dir_entry(const char *dir_path, const char *name)
{
    char path[STORAGE_PATH_MAX];
    bool is_dir = false;

    if (snprintf(path, sizeof(path), "%s/%s", dir_path, name) < (int)sizeof(path)) {
        is_dir = path_is_dir(path);
    }

    if (is_dir) {
        shell_puts("/");
    }
    shell_puts(name);
    shell_puts("\r\n");
}

static void shell_puts(const char *text)
{
    if (!text) {
        return;
    }
    shell_write_bytes(text, strlen(text));
}

static void shell_write_bytes(const void *data, size_t len)
{
    if (!data || len == 0) {
        return;
    }

    if (g_exec_io && g_exec_io->write) {
        g_exec_io->write(g_exec_io->ctx, data, len);
        return;
    }

    input_write_bytes(data, len);
}

static int shell_read_line(char *buf, size_t len)
{
    if (g_exec_io && g_exec_io->read_line) {
        return g_exec_io->read_line(g_exec_io->ctx, buf, len);
    }
    return input_read_line(buf, len);
}

static void shell_banner(void)
{
    shell_puts("\r\nC3 BASIC COMPUTER\r\nVersion 0.1\r\n\r\nREADY.\r\n");
    if (!storage_workspace_ready()) {
        shell_puts("WORKSPACE ERROR. Run RENEW to rebuild workspace.\r\n");
    }
}

static void shell_prompt(void)
{
    shell_puts("> ");
}

void shell_reset(void)
{
    snprintf(g_workspace_cwd, sizeof(g_workspace_cwd), "/");
}

void shell_register_external_exec(shell_external_exec_fn fn)
{
    g_external_exec = fn;
}

static void shell_help(void)
{
    shell_puts(
        "HELP PWD LS CD MKDIR RMDIR CAT WRITE RM CP MV EDIT\r\n"
        "RENEW\r\n");
}

static void shell_pwd(void)
{
    shell_puts(g_workspace_cwd);
    shell_puts("\r\n");
}

static bool shell_confirm(const char *prompt)
{
    char line[SHELL_BUF];
    shell_puts(prompt);
    shell_puts("\r\n");

    if (shell_read_line(line, sizeof(line)) <= 0) {
        return false;
    }

    const char *answer = skip_ws(line);
    if (answer == NULL || *answer == '\0') {
        return false;
    }

    return toupper((unsigned char)*answer) == 'Y';
}

static void shell_renew_workspace(void)
{
    if ((shell_exec_flags() & SHELL_EXEC_ALLOW_DESTRUCTIVE) == 0 ||
        (shell_exec_flags() & SHELL_EXEC_ALLOW_INTERACTIVE) == 0) {
        shell_error("Not allowed", SHELL_STATUS_NOT_ALLOWED);
        return;
    }

    shell_puts("WARNING.\r\n");
    shell_puts("This operation removes\r\n");
    shell_puts("all user programs and data.\r\n");
    shell_puts("The system software\r\n");
    shell_puts("will remain unchanged.\r\n");

    if (!shell_confirm("Do you understand? (Y/N)")) {
        shell_error("CANCELLED.", SHELL_STATUS_CANCELLED);
        return;
    }

    if (!shell_confirm("Ready to continue? (Y/N)")) {
        shell_error("CANCELLED.", SHELL_STATUS_CANCELLED);
        return;
    }

    if (storage_renew_workspace() != ESP_OK) {
        shell_error("RENEW failed", SHELL_STATUS_IO_ERROR);
        return;
    }

    shell_ok();
}

static void shell_ls(const char *path)
{
    char resolved[STORAGE_PATH_MAX];
    const char *arg = skip_ws(path);
    if (!storage_resolve_workspace_path(g_workspace_cwd, arg, resolved, sizeof(resolved))) {
        shell_error("Bad path", SHELL_STATUS_BAD_PATH);
        return;
    }

    DIR *dir = opendir(resolved);
    if (!dir) {
        shell_error("Cannot open directory", SHELL_STATUS_NOT_FOUND);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        shell_print_dir_entry(resolved, entry->d_name);
    }
    closedir(dir);
}

static void shell_cd(const char *path)
{
    char normalized[STORAGE_PATH_MAX];
    char resolved[STORAGE_PATH_MAX];
    if (!path || *skip_ws(path) == '\0' ||
        !storage_normalize_workspace_path(g_workspace_cwd, path, normalized, sizeof(normalized)) ||
        !storage_resolve_workspace_path(g_workspace_cwd, path, resolved, sizeof(resolved))) {
        shell_error("Bad path", SHELL_STATUS_BAD_PATH);
        return;
    }

    if (!path_is_dir(resolved)) {
        shell_error("Not directory", SHELL_STATUS_NOT_DIRECTORY);
        return;
    }

    snprintf(g_workspace_cwd, sizeof(g_workspace_cwd), "%s", normalized);
    shell_ok();
}

static void shell_mkdir(const char *path)
{
    char resolved[STORAGE_PATH_MAX];
    if (!resolve_workspace_arg(path, resolved, sizeof(resolved))) {
        shell_error("Bad path", SHELL_STATUS_BAD_PATH);
        return;
    }

    struct stat st;
    if (stat(resolved, &st) == 0) {
        shell_error("Exists", SHELL_STATUS_EXISTS);
        return;
    }

    if (mkdir(resolved, 0775) != 0) {
        shell_error("MKDIR failed", SHELL_STATUS_IO_ERROR);
        return;
    }

    shell_ok();
}

static void shell_rmdir(const char *path)
{
    char resolved[STORAGE_PATH_MAX];
    if (!resolve_workspace_arg(path, resolved, sizeof(resolved)) || !workspace_child_path(resolved)) {
        shell_error("Bad path", SHELL_STATUS_BAD_PATH);
        return;
    }

    if (!path_is_dir(resolved)) {
        shell_error("Not directory", SHELL_STATUS_NOT_DIRECTORY);
        return;
    }

    if (rmdir(resolved) != 0) {
        shell_error("RMDIR failed", SHELL_STATUS_IO_ERROR);
        return;
    }

    shell_ok();
}

static void shell_cat(const char *path)
{
    char resolved[STORAGE_PATH_MAX];
    if (!resolve_workspace_arg(path, resolved, sizeof(resolved))) {
        shell_error("Bad path", SHELL_STATUS_BAD_PATH);
        return;
    }

    if (path_is_dir(resolved)) {
        shell_error("Is directory", SHELL_STATUS_IS_DIRECTORY);
        return;
    }

    FILE *f = fopen(resolved, "r");
    if (!f) {
        shell_error("Open failed", SHELL_STATUS_NOT_FOUND);
        return;
    }

    char buf[FILE_IO_BUF];
    size_t n;
    char last = '\0';
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        shell_write_bytes(buf, n);
        last = buf[n - 1];
    }

    if (ferror(f)) {
        shell_error("\r\nRead failed", SHELL_STATUS_IO_ERROR);
    }
    fclose(f);
    if (last != '\n' && last != '\r') {
        shell_puts("\r\n");
    }
}

static void shell_write_file(char *args)
{
    bool force = false;
    char *cursor = args;
    char *path = next_token(&cursor);
    if (path && strcasecmp(path, "-F") == 0) {
        force = true;
        path = next_token(&cursor);
    }

    const char *text = skip_ws(cursor);
    bool use_stdin = !text || *text == '\0';
    if (!path || (use_stdin && (!shell_stdin_text() || shell_stdin_len() == 0))) {
        shell_error("Bad input", SHELL_STATUS_BAD_INPUT);
        return;
    }

    char resolved[STORAGE_PATH_MAX];
    if (!resolve_workspace_arg(path, resolved, sizeof(resolved))) {
        shell_error("Bad path", SHELL_STATUS_BAD_PATH);
        return;
    }

    struct stat st;
    if (stat(resolved, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            shell_error("Is directory", SHELL_STATUS_IS_DIRECTORY);
            return;
        }
        if (!force) {
            shell_error("Exists", SHELL_STATUS_EXISTS);
            return;
        }
    }

    FILE *f = fopen(resolved, "w");
    if (!f) {
        shell_error("WRITE failed", SHELL_STATUS_IO_ERROR);
        return;
    }

    size_t len = use_stdin ? shell_stdin_len() : strlen(text);
    text = use_stdin ? shell_stdin_text() : text;
    bool ok = fwrite(text, 1, len, f) == len;
    if (fclose(f) != 0) {
        ok = false;
    }
    if (!ok) {
        shell_error("WRITE failed", SHELL_STATUS_IO_ERROR);
        return;
    }

    shell_ok();
}

static bool remove_tree(const char *path)
{
    if (!workspace_child_path(path)) {
        return false;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }

    if (!S_ISDIR(st.st_mode)) {
        return remove(path) == 0;
    }

    DIR *dir = opendir(path);
    if (!dir) {
        return false;
    }

    bool ok = true;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char child[STORAGE_PATH_MAX];
        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) >= (int)sizeof(child) ||
            !workspace_child_path(child) ||
            !remove_tree(child)) {
            ok = false;
            break;
        }
    }
    closedir(dir);

    if (!ok) {
        return false;
    }
    return rmdir(path) == 0;
}

static void shell_rm(char *args)
{
    char *cursor = args;
    char *path = next_token(&cursor);
    bool recursive = false;

    if (path && strcasecmp(path, "-R") == 0) {
        recursive = true;
        path = next_token(&cursor);
    }

    char resolved[STORAGE_PATH_MAX];
    if (!path || !resolve_workspace_arg(path, resolved, sizeof(resolved)) || !workspace_child_path(resolved)) {
        shell_error("Bad path", SHELL_STATUS_BAD_PATH);
        return;
    }

    if (recursive) {
        if (!remove_tree(resolved)) {
            shell_error("RM failed", SHELL_STATUS_IO_ERROR);
            return;
        }
        shell_ok();
        return;
    }

    if (path_is_dir(resolved)) {
        shell_error("Is directory", SHELL_STATUS_IS_DIRECTORY);
        return;
    }

    if (remove(resolved) != 0) {
        shell_error("RM failed", SHELL_STATUS_IO_ERROR);
        return;
    }

    shell_ok();
}

static bool parse_two_paths(char *args, char **src, char **dst)
{
    char *cursor = args;
    *src = next_token(&cursor);
    *dst = next_token(&cursor);
    return *src && *dst;
}

static bool resolve_file_pair(char *args, char *src_path, size_t src_size, char *dst_path, size_t dst_size)
{
    char *src;
    char *dst;
    if (!parse_two_paths(args, &src, &dst)) {
        shell_error("Bad input", SHELL_STATUS_BAD_INPUT);
        return false;
    }

    if (!resolve_workspace_arg(src, src_path, src_size) ||
        !resolve_workspace_arg(dst, dst_path, dst_size)) {
        shell_error("Bad path", SHELL_STATUS_BAD_PATH);
        return false;
    }

    struct stat st;
    if (stat(src_path, &st) != 0) {
        shell_error("Open failed", SHELL_STATUS_NOT_FOUND);
        return false;
    }

    if (S_ISDIR(st.st_mode) || path_is_dir(dst_path)) {
        shell_error("Is directory", SHELL_STATUS_IS_DIRECTORY);
        return false;
    }

    if (stat(dst_path, &st) == 0) {
        shell_error("Exists", SHELL_STATUS_EXISTS);
        return false;
    }

    return true;
}

static void shell_copy(char *args)
{
    char src_path[STORAGE_PATH_MAX];
    char dst_path[STORAGE_PATH_MAX];
    if (!resolve_file_pair(args, src_path, sizeof(src_path), dst_path, sizeof(dst_path))) {
        return;
    }

    FILE *src = fopen(src_path, "r");
    if (!src) {
        shell_error("Open failed", SHELL_STATUS_NOT_FOUND);
        return;
    }

    FILE *dst = fopen(dst_path, "w");
    if (!dst) {
        fclose(src);
        shell_error("CP failed", SHELL_STATUS_IO_ERROR);
        return;
    }

    char buf[FILE_IO_BUF];
    bool ok = true;
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        if (fwrite(buf, 1, n, dst) != n) {
            ok = false;
            break;
        }
    }
    if (ferror(src)) {
        ok = false;
    }

    if (fclose(dst) != 0) {
        ok = false;
    }
    fclose(src);

    if (!ok) {
        remove(dst_path);
        shell_error("CP failed", SHELL_STATUS_IO_ERROR);
        return;
    }

    shell_ok();
}

static void shell_move(char *args)
{
    char *src;
    char *dst;
    char src_path[STORAGE_PATH_MAX];
    char dst_path[STORAGE_PATH_MAX];

    if (!parse_two_paths(args, &src, &dst)) {
        shell_error("Bad input", SHELL_STATUS_BAD_INPUT);
        return;
    }

    if (!resolve_workspace_arg(src, src_path, sizeof(src_path)) ||
        !resolve_workspace_arg(dst, dst_path, sizeof(dst_path)) ||
        !workspace_child_path(src_path) ||
        !workspace_child_path(dst_path)) {
        shell_error("Bad path", SHELL_STATUS_BAD_PATH);
        return;
    }

    struct stat st;
    if (stat(src_path, &st) != 0) {
        shell_error("Open failed", SHELL_STATUS_NOT_FOUND);
        return;
    }

    if (stat(dst_path, &st) == 0) {
        shell_error("Exists", SHELL_STATUS_EXISTS);
        return;
    }

    if (path_is_dir(src_path) && path_is_inside(src_path, dst_path)) {
        shell_error("Bad path", SHELL_STATUS_BAD_PATH);
        return;
    }

    if (rename(src_path, dst_path) != 0) {
        shell_error("MV failed", SHELL_STATUS_IO_ERROR);
        return;
    }

    shell_ok();
}

static void shell_dispatch_command(shell_command_t command, char *arg)
{
    switch (command) {
    case SHELL_COMMAND_HELP:
        shell_help();
        break;
    case SHELL_COMMAND_PWD:
        shell_pwd();
        break;
    case SHELL_COMMAND_LS:
        shell_ls(arg);
        break;
    case SHELL_COMMAND_CD:
        shell_cd(arg);
        break;
    case SHELL_COMMAND_MKDIR:
        shell_mkdir(arg);
        break;
    case SHELL_COMMAND_RMDIR:
        shell_rmdir(arg);
        break;
    case SHELL_COMMAND_CAT:
        shell_cat(arg);
        break;
    case SHELL_COMMAND_WRITE:
        shell_write_file(arg);
        break;
    case SHELL_COMMAND_RM:
        shell_rm(arg);
        break;
    case SHELL_COMMAND_CP:
        shell_copy(arg);
        break;
    case SHELL_COMMAND_MV:
        shell_move(arg);
        break;
    case SHELL_COMMAND_RENEW:
        shell_renew_workspace();
        break;
    }
}

static shell_status_t shell_run_with_io(const shell_exec_io_t *io, shell_command_t command, const char *args)
{
    char arg_buf[SHELL_BUF];
    const shell_exec_io_t *prev_io = g_exec_io;
    shell_status_t prev_status = g_exec_status;

    g_exec_io = io;
    g_exec_status = SHELL_STATUS_OK;

    if (args) {
        strncpy(arg_buf, args, sizeof(arg_buf) - 1);
    } else {
        arg_buf[0] = '\0';
    }
    arg_buf[sizeof(arg_buf) - 1] = '\0';

    shell_dispatch_command(command, arg_buf);

    shell_status_t result = g_exec_status;
    g_exec_io = prev_io;
    g_exec_status = prev_status;
    return result;
}

shell_status_t shell_command_from_name(const char *name, shell_command_t *out)
{
    if (!name || !out || *skip_ws(name) == '\0') {
        return SHELL_STATUS_BAD_INPUT;
    }

    if (strcasecmp(name, "HELP") == 0) {
        *out = SHELL_COMMAND_HELP;
    } else if (strcasecmp(name, "PWD") == 0) {
        *out = SHELL_COMMAND_PWD;
    } else if (strcasecmp(name, "LS") == 0) {
        *out = SHELL_COMMAND_LS;
    } else if (strcasecmp(name, "CD") == 0) {
        *out = SHELL_COMMAND_CD;
    } else if (strcasecmp(name, "MKDIR") == 0) {
        *out = SHELL_COMMAND_MKDIR;
    } else if (strcasecmp(name, "RMDIR") == 0) {
        *out = SHELL_COMMAND_RMDIR;
    } else if (strcasecmp(name, "CAT") == 0) {
        *out = SHELL_COMMAND_CAT;
    } else if (strcasecmp(name, "WRITE") == 0) {
        *out = SHELL_COMMAND_WRITE;
    } else if (strcasecmp(name, "RM") == 0) {
        *out = SHELL_COMMAND_RM;
    } else if (strcasecmp(name, "CP") == 0) {
        *out = SHELL_COMMAND_CP;
    } else if (strcasecmp(name, "MV") == 0) {
        *out = SHELL_COMMAND_MV;
    } else if (strcasecmp(name, "RENEW") == 0) {
        *out = SHELL_COMMAND_RENEW;
    } else {
        return SHELL_STATUS_UNKNOWN_COMMAND;
    }

    return SHELL_STATUS_OK;
}

shell_status_t shell_exec_command(shell_command_t command, const char *args, const shell_exec_io_t *io)
{
    if (command < SHELL_COMMAND_HELP || command > SHELL_COMMAND_RENEW) {
        return SHELL_STATUS_UNKNOWN_COMMAND;
    }
    return shell_run_with_io(io, command, args);
}

typedef struct {
    char *buf;
    size_t cap;
    size_t len;
    bool overflow;
} shell_capture_t;

static void shell_capture_write(void *ctx, const void *data, size_t len)
{
    shell_capture_t *capture = (shell_capture_t *)ctx;
    if (!capture || !data || len == 0 || capture->cap == 0) {
        return;
    }

    size_t room = capture->cap - capture->len - 1;
    if (len > room) {
        len = room;
        capture->overflow = true;
    }

    if (len > 0) {
        memcpy(capture->buf + capture->len, data, len);
        capture->len += len;
        capture->buf[capture->len] = '\0';
    }
}

static bool split_one_pipe(const char *line, char *left, size_t left_size, char *right, size_t right_size)
{
    const char *pipe = strchr(line, '|');
    if (!pipe) {
        return false;
    }
    if (strchr(pipe + 1, '|')) {
        return false;
    }

    size_t left_len = (size_t)(pipe - line);
    const char *right_start = pipe + 1;
    while (left_len > 0 && isspace((unsigned char)line[left_len - 1])) {
        left_len--;
    }
    while (*right_start && isspace((unsigned char)*right_start)) {
        right_start++;
    }

    if (left_len == 0 || *right_start == '\0' || left_len >= left_size || strlen(right_start) >= right_size) {
        return false;
    }

    memcpy(left, line, left_len);
    left[left_len] = '\0';
    strncpy(right, right_start, right_size - 1);
    right[right_size - 1] = '\0';
    return true;
}

static shell_status_t shell_exec_pipe(const char *line, const shell_exec_io_t *io)
{
    char left[SHELL_BUF];
    char right[SHELL_BUF];
    char *pipe_buf = calloc(1, SHELL_PIPE_BUF);
    shell_capture_t capture = {
        .buf = pipe_buf,
        .cap = SHELL_PIPE_BUF,
        .len = 0,
        .overflow = false,
    };

    if (!pipe_buf) {
        const shell_exec_io_t *prev_io = g_exec_io;
        shell_status_t prev_status = g_exec_status;
        g_exec_io = io;
        g_exec_status = SHELL_STATUS_OK;
        shell_error("Pipe memory failed", SHELL_STATUS_IO_ERROR);
        g_exec_io = prev_io;
        g_exec_status = prev_status;
        return SHELL_STATUS_IO_ERROR;
    }

    if (!split_one_pipe(line, left, sizeof(left), right, sizeof(right))) {
        const shell_exec_io_t *prev_io = g_exec_io;
        shell_status_t prev_status = g_exec_status;
        g_exec_io = io;
        g_exec_status = SHELL_STATUS_OK;
        shell_error("Bad pipe", SHELL_STATUS_BAD_INPUT);
        g_exec_io = prev_io;
        g_exec_status = prev_status;
        free(pipe_buf);
        return SHELL_STATUS_BAD_INPUT;
    }

    shell_exec_io_t left_io = {
        .write = shell_capture_write,
        .read_line = io ? io->read_line : NULL,
        .ctx = &capture,
        .flags = io ? io->flags : 0,
    };

    shell_status_t left_status = shell_exec_line_no_pipe(left, &left_io);
    if (left_status != SHELL_STATUS_OK && left_status != SHELL_STATUS_EMPTY) {
        free(pipe_buf);
        return left_status;
    }
    if (capture.overflow) {
        const shell_exec_io_t *prev_io = g_exec_io;
        shell_status_t prev_status = g_exec_status;
        g_exec_io = io;
        g_exec_status = SHELL_STATUS_OK;
        shell_error("Pipe full", SHELL_STATUS_IO_ERROR);
        g_exec_io = prev_io;
        g_exec_status = prev_status;
        free(pipe_buf);
        return SHELL_STATUS_IO_ERROR;
    }

    shell_exec_io_t right_io = {
        .write = io ? io->write : NULL,
        .read_line = io ? io->read_line : NULL,
        .ctx = io ? io->ctx : NULL,
        .flags = io ? io->flags : 0,
        .stdin_text = pipe_buf,
        .stdin_len = capture.len,
    };

    shell_status_t right_status = shell_exec_line_no_pipe(right, &right_io);
    free(pipe_buf);
    return right_status;
}

static shell_status_t shell_exec_line_no_pipe(const char *line, const shell_exec_io_t *io)
{
    const char *trimmed = line;
    if (!trimmed) {
        return SHELL_STATUS_BAD_INPUT;
    }

    while (*trimmed && isspace((unsigned char)*trimmed)) {
        trimmed++;
    }

    if (*trimmed == '\0') {
        return SHELL_STATUS_EMPTY;
    }

    char command[SHELL_BUF];
    strncpy(command, trimmed, sizeof(command) - 1);
    command[sizeof(command) - 1] = '\0';

    char *arg = command;
    while (*arg && !isspace((unsigned char)*arg)) {
        arg++;
    }
    if (*arg) {
        *arg++ = '\0';
    }
    while (*arg && isspace((unsigned char)*arg)) {
        arg++;
    }

    shell_command_t parsed;
    shell_status_t status = shell_command_from_name(command, &parsed);
    if (status != SHELL_STATUS_OK) {
        if (g_external_exec) {
            status = g_external_exec(trimmed, io);
            if (status != SHELL_STATUS_UNKNOWN_COMMAND) {
                return status;
            }
        }

        const shell_exec_io_t *prev_io = g_exec_io;
        shell_status_t prev_status = g_exec_status;
        g_exec_io = io;
        g_exec_status = SHELL_STATUS_OK;
        shell_error("UNKNOWN COMMAND", SHELL_STATUS_UNKNOWN_COMMAND);
        g_exec_io = prev_io;
        g_exec_status = prev_status;
        return SHELL_STATUS_UNKNOWN_COMMAND;
    }

    return shell_run_with_io(io, parsed, arg);
}

shell_status_t shell_exec_line(const char *line, const shell_exec_io_t *io)
{
    if (line && strchr(line, '|')) {
        return shell_exec_pipe(line, io);
    }
    return shell_exec_line_no_pipe(line, io);
}

const char *shell_status_name(shell_status_t status)
{
    switch (status) {
    case SHELL_STATUS_OK:
        return "OK";
    case SHELL_STATUS_EMPTY:
        return "EMPTY";
    case SHELL_STATUS_UNKNOWN_COMMAND:
        return "UNKNOWN_COMMAND";
    case SHELL_STATUS_BAD_INPUT:
        return "BAD_INPUT";
    case SHELL_STATUS_BAD_PATH:
        return "BAD_PATH";
    case SHELL_STATUS_NOT_FOUND:
        return "NOT_FOUND";
    case SHELL_STATUS_EXISTS:
        return "EXISTS";
    case SHELL_STATUS_IS_DIRECTORY:
        return "IS_DIRECTORY";
    case SHELL_STATUS_NOT_DIRECTORY:
        return "NOT_DIRECTORY";
    case SHELL_STATUS_IO_ERROR:
        return "IO_ERROR";
    case SHELL_STATUS_CANCELLED:
        return "CANCELLED";
    case SHELL_STATUS_NOT_ALLOWED:
        return "NOT_ALLOWED";
    default:
        return "UNKNOWN_STATUS";
    }
}

void shell_run(void)
{
    ESP_ERROR_CHECK(input_init());

    shell_exec_io_t interactive_io = {
        .write = NULL,
        .read_line = NULL,
        .ctx = NULL,
        .flags = SHELL_EXEC_ALLOW_INTERACTIVE | SHELL_EXEC_ALLOW_DESTRUCTIVE,
    };

    shell_reset();
    shell_banner();

    char line[SHELL_BUF];
    while (1) {
        shell_prompt();
        if (input_read_line(line, sizeof(line)) <= 0) {
            continue;
        }
        shell_exec_line(line, &interactive_io);
    }
}
//Keep Going.
