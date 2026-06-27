#include "shell.h"

#include "c3_com.h"
#include "input.h"
#include "storage.h"

#include <ctype.h>
#include <dirent.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define SHELL_BUF 256
#define FILE_IO_BUF 128
#define SHELL_PIPE_BUF 2048
#define YMODEM_PACKET_128 128
#define YMODEM_PACKET_1K 1024
#define YMODEM_TIMEOUT_MS 10000
#define YMODEM_START_RETRIES 16
#define YMODEM_MAX_FILE_SIZE (512u * 1024u)
#define C3_COM_ARG_MAX 8
#define SHELL_GLOB_MATCH_MAX 32

enum {
    YMODEM_SOH = 0x01,
    YMODEM_STX = 0x02,
    YMODEM_EOT = 0x04,
    YMODEM_ACK = 0x06,
    YMODEM_NAK = 0x15,
    YMODEM_CAN = 0x18,
    YMODEM_CRC_REQ = 'C',
    YMODEM_PAD = 0x1A,
};

static char g_workspace_cwd[STORAGE_PATH_MAX];
static const shell_exec_io_t *g_exec_io;
static shell_status_t g_exec_status;
static shell_external_exec_fn g_external_exec;
static char g_glob_matches[SHELL_GLOB_MATCH_MAX][STORAGE_PATH_MAX];

static void shell_puts(const char *text);
static void shell_write_bytes(const void *data, size_t len);
static shell_status_t shell_exec_line_no_pipe(const char *line, const shell_exec_io_t *io);
static const char *path_basename(const char *path);

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

static bool shell_has_wildcard(const char *text)
{
    return text && (strchr(text, '*') || strchr(text, '?'));
}

static bool shell_glob_match(const char *pattern, const char *text)
{
    if (!pattern || !text) {
        return false;
    }

    while (*pattern) {
        if (*pattern == '*') {
            while (*pattern == '*') {
                pattern++;
            }
            if (*pattern == '\0') {
                return true;
            }
            while (*text) {
                if (shell_glob_match(pattern, text)) {
                    return true;
                }
                text++;
            }
            return false;
        }

        if (*text == '\0') {
            return false;
        }

        if (*pattern != '?' &&
            toupper((unsigned char)*pattern) != toupper((unsigned char)*text)) {
            return false;
        }
        pattern++;
        text++;
    }

    return *text == '\0';
}

static bool shell_split_pattern(const char *arg, char *dir_arg, size_t dir_arg_size,
                                char *pattern, size_t pattern_size)
{
    char normalized[STORAGE_PATH_MAX];
    if (!storage_normalize_workspace_path(g_workspace_cwd, arg, normalized, sizeof(normalized))) {
        return false;
    }

    const char *last_sep = strrchr(normalized, '/');
    const char *name = last_sep ? last_sep + 1 : normalized;
    if (!name || *name == '\0' || !shell_has_wildcard(name)) {
        return false;
    }

    size_t dir_len = last_sep ? (size_t)(last_sep - normalized) : 0;
    if (dir_len == 0) {
        dir_len = 1;
    }
    if (dir_len >= dir_arg_size || strlen(name) >= pattern_size) {
        return false;
    }

    memcpy(dir_arg, normalized, dir_len);
    dir_arg[dir_len] = '\0';
    snprintf(pattern, pattern_size, "%s", name);
    return true;
}

static bool shell_collect_glob(const char *arg, char matches[][STORAGE_PATH_MAX],
                               size_t match_cap, size_t *match_count)
{
    char dir_arg[STORAGE_PATH_MAX];
    char pattern[STORAGE_PATH_MAX];
    char dir_path[STORAGE_PATH_MAX];

    if (!match_count ||
        !shell_split_pattern(arg, dir_arg, sizeof(dir_arg), pattern, sizeof(pattern)) ||
        !storage_resolve_workspace_path(g_workspace_cwd, dir_arg, dir_path, sizeof(dir_path))) {
        return false;
    }

    DIR *dir = opendir(dir_path);
    if (!dir) {
        return false;
    }

    *match_count = 0;
    bool overflow = false;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (!shell_glob_match(pattern, entry->d_name)) {
            continue;
        }
        if (*match_count >= match_cap) {
            overflow = true;
            break;
        }
        if (strcmp(dir_path, STORAGE_WORKSPACE_MOUNT_POINT) == 0) {
            if (snprintf(matches[*match_count], STORAGE_PATH_MAX, "%s/%s",
                         STORAGE_WORKSPACE_MOUNT_POINT, entry->d_name) >= STORAGE_PATH_MAX) {
                overflow = true;
                break;
            }
        } else if (snprintf(matches[*match_count], STORAGE_PATH_MAX, "%s/%s",
                            dir_path, entry->d_name) >= STORAGE_PATH_MAX) {
            overflow = true;
            break;
        }
        (*match_count)++;
    }
    closedir(dir);

    return !overflow;
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
        "HELP DF PWD LS CD MKDIR RMDIR CAT WRITE RM CP MV RECV SEND RUN EDIT\r\n"
        "RENEW\r\n");
}

static size_t bytes_to_1k_blocks(size_t bytes)
{
    return (bytes + 1023u) / 1024u;
}

static void shell_df(const char *args)
{
    if (args && *skip_ws(args) != '\0') {
        shell_error("Usage: DF", SHELL_STATUS_BAD_INPUT);
        return;
    }

    size_t total = 0;
    size_t used = 0;
    size_t free_bytes = 0;
    if (!storage_workspace_usage_bytes(&total, &used, &free_bytes)) {
        shell_error("DF failed", SHELL_STATUS_IO_ERROR);
        return;
    }

    shell_puts("Filesystem 1K-blocks Used Available Mounted on\r\n");
    char line[96];
    snprintf(line, sizeof(line), "workspace_fs %u %u %u /\r\n",
             (unsigned)bytes_to_1k_blocks(total),
             (unsigned)bytes_to_1k_blocks(used),
             (unsigned)bytes_to_1k_blocks(free_bytes));
    shell_puts(line);
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
    if (shell_has_wildcard(arg)) {
        size_t match_count = 0;
        if (!shell_collect_glob(arg, g_glob_matches, SHELL_GLOB_MATCH_MAX, &match_count)) {
            shell_error("Bad path", SHELL_STATUS_BAD_PATH);
            return;
        }
        if (match_count == 0) {
            shell_error("No match", SHELL_STATUS_NOT_FOUND);
            return;
        }
        for (size_t i = 0; i < match_count; ++i) {
            const char *name = path_basename(g_glob_matches[i]);
            if (path_is_dir(g_glob_matches[i])) {
                shell_puts("/");
            }
            shell_puts(name);
            shell_puts("\r\n");
        }
        return;
    }

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
    const char *arg = skip_ws(path);
    if (shell_has_wildcard(arg)) {
        size_t match_count = 0;
        if (!shell_collect_glob(arg, g_glob_matches, SHELL_GLOB_MATCH_MAX, &match_count)) {
            shell_error("Bad path", SHELL_STATUS_BAD_PATH);
            return;
        }
        if (match_count == 0) {
            shell_error("No match", SHELL_STATUS_NOT_FOUND);
            return;
        }
        for (size_t i = 0; i < match_count; ++i) {
            if (path_is_dir(g_glob_matches[i])) {
                shell_error("Is directory", SHELL_STATUS_IS_DIRECTORY);
                return;
            }
            shell_cat(g_glob_matches[i] + strlen(STORAGE_WORKSPACE_MOUNT_POINT));
        }
        return;
    }

    if (!resolve_workspace_arg(arg, resolved, sizeof(resolved))) {
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

    if (!path) {
        shell_error("Bad path", SHELL_STATUS_BAD_PATH);
        return;
    }

    if (shell_has_wildcard(path)) {
        size_t match_count = 0;
        if (!shell_collect_glob(path, g_glob_matches, SHELL_GLOB_MATCH_MAX, &match_count)) {
            shell_error("Bad path", SHELL_STATUS_BAD_PATH);
            return;
        }
        if (match_count == 0) {
            shell_error("No match", SHELL_STATUS_NOT_FOUND);
            return;
        }

        for (size_t i = 0; i < match_count; ++i) {
            if (!workspace_child_path(g_glob_matches[i])) {
                shell_error("Bad path", SHELL_STATUS_BAD_PATH);
                return;
            }
            if (!recursive && path_is_dir(g_glob_matches[i])) {
                shell_error("Is directory", SHELL_STATUS_IS_DIRECTORY);
                return;
            }
        }

        for (size_t i = 0; i < match_count; ++i) {
            bool ok = recursive ? remove_tree(g_glob_matches[i]) : (remove(g_glob_matches[i]) == 0);
            if (!ok) {
                shell_error("RM failed", SHELL_STATUS_IO_ERROR);
                return;
            }
        }
        shell_ok();
        return;
    }

    char resolved[STORAGE_PATH_MAX];
    if (!resolve_workspace_arg(path, resolved, sizeof(resolved)) || !workspace_child_path(resolved)) {
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

static bool copy_file_path(const char *src_path, const char *dst_path)
{
    FILE *src = fopen(src_path, "r");
    if (!src) {
        shell_error("Open failed", SHELL_STATUS_NOT_FOUND);
        return false;
    }

    FILE *dst = fopen(dst_path, "w");
    if (!dst) {
        fclose(src);
        shell_error("CP failed", SHELL_STATUS_IO_ERROR);
        return false;
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
        return false;
    }

    return true;
}

static bool resolve_wildcard_destination(const char *dst, char *dst_dir, size_t dst_dir_size)
{
    if (!dst || shell_has_wildcard(dst) ||
        !resolve_workspace_arg(dst, dst_dir, dst_dir_size) ||
        !workspace_child_path(dst_dir) ||
        !path_is_dir(dst_dir)) {
        shell_error("Bad path", SHELL_STATUS_BAD_PATH);
        return false;
    }
    return true;
}

static void shell_copy(char *args)
{
    char *src_arg;
    char *dst_arg;
    if (!parse_two_paths(args, &src_arg, &dst_arg)) {
        shell_error("Bad input", SHELL_STATUS_BAD_INPUT);
        return;
    }

    if (shell_has_wildcard(src_arg)) {
        size_t match_count = 0;
        char dst_dir[STORAGE_PATH_MAX];
        if (!shell_collect_glob(src_arg, g_glob_matches, SHELL_GLOB_MATCH_MAX, &match_count)) {
            shell_error("Bad path", SHELL_STATUS_BAD_PATH);
            return;
        }
        if (match_count == 0) {
            shell_error("No match", SHELL_STATUS_NOT_FOUND);
            return;
        }
        if (!resolve_wildcard_destination(dst_arg, dst_dir, sizeof(dst_dir))) {
            return;
        }

        for (size_t i = 0; i < match_count; ++i) {
            char dst_path[STORAGE_PATH_MAX];
            if (path_is_dir(g_glob_matches[i])) {
                shell_error("Is directory", SHELL_STATUS_IS_DIRECTORY);
                return;
            }
            if (snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir,
                         path_basename(g_glob_matches[i])) >= STORAGE_PATH_MAX) {
                shell_error("Bad path", SHELL_STATUS_BAD_PATH);
                return;
            }
            struct stat st;
            if (stat(dst_path, &st) == 0) {
                shell_error("Exists", SHELL_STATUS_EXISTS);
                return;
            }
        }

        for (size_t i = 0; i < match_count; ++i) {
            char dst_path[STORAGE_PATH_MAX];
            if (snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir,
                         path_basename(g_glob_matches[i])) >= STORAGE_PATH_MAX ||
                !copy_file_path(g_glob_matches[i], dst_path)) {
                return;
            }
        }
        shell_ok();
        return;
    }

    char src_path[STORAGE_PATH_MAX];
    char dst_path[STORAGE_PATH_MAX];
    if (!resolve_workspace_arg(src_arg, src_path, sizeof(src_path)) ||
        !resolve_workspace_arg(dst_arg, dst_path, sizeof(dst_path))) {
        shell_error("Bad path", SHELL_STATUS_BAD_PATH);
        return;
    }

    struct stat st;
    if (stat(src_path, &st) != 0) {
        shell_error("Open failed", SHELL_STATUS_NOT_FOUND);
        return;
    }

    if (S_ISDIR(st.st_mode) || path_is_dir(dst_path)) {
        shell_error("Is directory", SHELL_STATUS_IS_DIRECTORY);
        return;
    }

    if (stat(dst_path, &st) == 0) {
        shell_error("Exists", SHELL_STATUS_EXISTS);
        return;
    }

    if (!copy_file_path(src_path, dst_path)) {
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

    if (shell_has_wildcard(src)) {
        size_t match_count = 0;
        char dst_dir[STORAGE_PATH_MAX];
        if (!shell_collect_glob(src, g_glob_matches, SHELL_GLOB_MATCH_MAX, &match_count)) {
            shell_error("Bad path", SHELL_STATUS_BAD_PATH);
            return;
        }
        if (match_count == 0) {
            shell_error("No match", SHELL_STATUS_NOT_FOUND);
            return;
        }
        if (!resolve_wildcard_destination(dst, dst_dir, sizeof(dst_dir))) {
            return;
        }

        for (size_t i = 0; i < match_count; ++i) {
            char dst_path[STORAGE_PATH_MAX];
            if (path_is_dir(g_glob_matches[i])) {
                shell_error("Is directory", SHELL_STATUS_IS_DIRECTORY);
                return;
            }
            if (snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir,
                         path_basename(g_glob_matches[i])) >= STORAGE_PATH_MAX) {
                shell_error("Bad path", SHELL_STATUS_BAD_PATH);
                return;
            }
            struct stat st;
            if (stat(dst_path, &st) == 0) {
                shell_error("Exists", SHELL_STATUS_EXISTS);
                return;
            }
        }

        for (size_t i = 0; i < match_count; ++i) {
            char dst_path[STORAGE_PATH_MAX];
            if (snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir,
                         path_basename(g_glob_matches[i])) >= STORAGE_PATH_MAX ||
                rename(g_glob_matches[i], dst_path) != 0) {
                shell_error("MV failed", SHELL_STATUS_IO_ERROR);
                return;
            }
        }
        shell_ok();
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

static uint16_t ymodem_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static void ymodem_put_byte(uint8_t ch)
{
    shell_write_bytes(&ch, 1);
}

static void ymodem_write_bytes(const uint8_t *data, size_t len)
{
    while (len > 0) {
        size_t chunk = len > 64 ? 64 : len;
        shell_write_bytes(data, chunk);
        data += chunk;
        len -= chunk;
    }
}

static bool ymodem_read_exact(uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    size_t used = 0;
    while (used < len) {
        int n = input_read_bytes(buf + used, len - used, timeout_ms);
        if (n <= 0) {
            return false;
        }
        used += (size_t)n;
    }
    return true;
}

static int ymodem_read_byte(uint32_t timeout_ms)
{
    uint8_t ch;
    if (!ymodem_read_exact(&ch, 1, timeout_ms)) {
        return -1;
    }
    return ch;
}

static int ymodem_read_packet(uint8_t *data, size_t *data_len, uint8_t *block_no)
{
    int first = ymodem_read_byte(YMODEM_TIMEOUT_MS);
    if (first < 0) {
        return 0;
    }
    if (first == YMODEM_EOT) {
        return 2;
    }
    if (first == YMODEM_CAN) {
        int second = ymodem_read_byte(1000);
        return second == YMODEM_CAN ? 3 : -1;
    }
    if (first != YMODEM_SOH && first != YMODEM_STX) {
        return -1;
    }

    size_t len = first == YMODEM_SOH ? YMODEM_PACKET_128 : YMODEM_PACKET_1K;
    uint8_t header[2];
    uint8_t crc_bytes[2];
    if (!ymodem_read_exact(header, sizeof(header), YMODEM_TIMEOUT_MS) ||
        !ymodem_read_exact(data, len, YMODEM_TIMEOUT_MS) ||
        !ymodem_read_exact(crc_bytes, sizeof(crc_bytes), YMODEM_TIMEOUT_MS)) {
        return -1;
    }

    if ((uint8_t)(header[0] + header[1]) != 0xFF) {
        return -1;
    }

    uint16_t got_crc = ((uint16_t)crc_bytes[0] << 8) | crc_bytes[1];
    if (ymodem_crc16(data, len) != got_crc) {
        return -1;
    }

    *block_no = header[0];
    *data_len = len;
    return 1;
}

static bool ymodem_parse_size(const uint8_t *block, size_t *file_size)
{
    size_t name_len = strnlen((const char *)block, YMODEM_PACKET_128);
    if (name_len == 0 || name_len + 1 >= YMODEM_PACKET_128) {
        return false;
    }

    const char *size_text = (const char *)block + name_len + 1;
    if (*size_text == '\0') {
        return false;
    }

    char *end = NULL;
    unsigned long size = strtoul(size_text, &end, 10);
    if (end == size_text || size > YMODEM_MAX_FILE_SIZE) {
        return false;
    }

    *file_size = (size_t)size;
    return true;
}

static const char *path_basename(const char *path)
{
    const char *base = strrchr(path, '/');
    return base ? base + 1 : path;
}

static shell_status_t ymodem_wait_for_crc_request(void)
{
    for (int i = 0; i < YMODEM_START_RETRIES; i++) {
        int ch = ymodem_read_byte(YMODEM_TIMEOUT_MS);
        if (ch == YMODEM_CRC_REQ) {
            return SHELL_STATUS_OK;
        }
        if (ch == YMODEM_CAN) {
            int second = ymodem_read_byte(1000);
            if (second == YMODEM_CAN) {
                return SHELL_STATUS_CANCELLED;
            }
        }
    }
    return SHELL_STATUS_IO_ERROR;
}

static shell_status_t ymodem_send_packet(uint8_t block_no, const uint8_t *data, size_t len)
{
    uint8_t *packet = malloc(len + 5);
    if (!packet) {
        return SHELL_STATUS_IO_ERROR;
    }

    packet[0] = len == YMODEM_PACKET_128 ? YMODEM_SOH : YMODEM_STX;
    packet[1] = block_no;
    packet[2] = (uint8_t)~block_no;
    memcpy(packet + 3, data, len);
    uint16_t crc = ymodem_crc16(data, len);
    packet[3 + len] = (uint8_t)(crc >> 8);
    packet[4 + len] = (uint8_t)crc;
    ymodem_write_bytes(packet, len + 5);
    free(packet);

    while (true) {
        int reply = ymodem_read_byte(YMODEM_TIMEOUT_MS);
        if (reply == YMODEM_ACK) {
            return SHELL_STATUS_OK;
        }
        if (reply == YMODEM_CRC_REQ) {
            continue;
        }
        if (reply == YMODEM_CAN) {
            int second = ymodem_read_byte(1000);
            if (second == YMODEM_CAN) {
                return SHELL_STATUS_CANCELLED;
            }
        }
        return SHELL_STATUS_IO_ERROR;
    }
}

static shell_status_t shell_recv_file(char *args)
{
    if ((shell_exec_flags() & SHELL_EXEC_ALLOW_INTERACTIVE) == 0) {
        shell_error("Not allowed", SHELL_STATUS_NOT_ALLOWED);
        return SHELL_STATUS_NOT_ALLOWED;
    }

    bool force = false;
    char *cursor = args;
    char *path = next_token(&cursor);
    if (path && strcasecmp(path, "-F") == 0) {
        force = true;
        path = next_token(&cursor);
    }

    if (!path || *skip_ws(cursor) != '\0') {
        shell_error("Usage: RECV [-F] <path>", SHELL_STATUS_BAD_INPUT);
        return SHELL_STATUS_BAD_INPUT;
    }

    char resolved[STORAGE_PATH_MAX];
    if (!resolve_workspace_arg(path, resolved, sizeof(resolved)) || !workspace_child_path(resolved)) {
        shell_error("Bad path", SHELL_STATUS_BAD_PATH);
        return SHELL_STATUS_BAD_PATH;
    }

    struct stat st;
    size_t existing_size = 0;
    if (stat(resolved, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            shell_error("Is directory", SHELL_STATUS_IS_DIRECTORY);
            return SHELL_STATUS_IS_DIRECTORY;
        }
        if (!force) {
            shell_error("Exists", SHELL_STATUS_EXISTS);
            return SHELL_STATUS_EXISTS;
        }
        if (st.st_size > 0) {
            existing_size = (size_t)st.st_size;
        }
    }

    uint8_t *block = malloc(YMODEM_PACKET_1K);
    if (!block) {
        shell_error("Out of memory", SHELL_STATUS_IO_ERROR);
        return SHELL_STATUS_IO_ERROR;
    }

    size_t block_len = 0;
    uint8_t block_no = 0;
    int packet_status = 0;
    for (int i = 0; i < YMODEM_START_RETRIES; i++) {
        ymodem_put_byte(YMODEM_CRC_REQ);
        packet_status = ymodem_read_packet(block, &block_len, &block_no);
        if (packet_status == 1) {
            break;
        }
        if (packet_status == 3) {
            free(block);
            shell_error("Transfer cancelled", SHELL_STATUS_CANCELLED);
            return SHELL_STATUS_CANCELLED;
        }
    }

    if (packet_status != 1 || block_no != 0 || block_len != YMODEM_PACKET_128) {
        free(block);
        shell_error("YMODEM header failed", SHELL_STATUS_IO_ERROR);
        return SHELL_STATUS_IO_ERROR;
    }

    size_t file_size = 0;
    if (!ymodem_parse_size(block, &file_size)) {
        ymodem_put_byte(YMODEM_CAN);
        ymodem_put_byte(YMODEM_CAN);
        free(block);
        shell_error("YMODEM size missing", SHELL_STATUS_BAD_INPUT);
        return SHELL_STATUS_BAD_INPUT;
    }

    size_t free_bytes = 0;
    if (!storage_workspace_free_bytes(&free_bytes) ||
        file_size > free_bytes + existing_size) {
        ymodem_put_byte(YMODEM_CAN);
        ymodem_put_byte(YMODEM_CAN);
        free(block);
        shell_error("Not enough space", SHELL_STATUS_IO_ERROR);
        return SHELL_STATUS_IO_ERROR;
    }

    FILE *f = fopen(resolved, "wb");
    if (!f) {
        ymodem_put_byte(YMODEM_CAN);
        ymodem_put_byte(YMODEM_CAN);
        free(block);
        shell_error("RECV failed", SHELL_STATUS_IO_ERROR);
        return SHELL_STATUS_IO_ERROR;
    }

    ymodem_put_byte(YMODEM_ACK);
    ymodem_put_byte(YMODEM_CRC_REQ);

    size_t written = 0;
    uint8_t expected = 1;
    bool ok = true;
    while (written < file_size) {
        packet_status = ymodem_read_packet(block, &block_len, &block_no);
        if (packet_status != 1 || block_no != expected) {
            ok = false;
            break;
        }

        size_t remaining = file_size - written;
        size_t to_write = remaining < block_len ? remaining : block_len;
        if (to_write > 0 && fwrite(block, 1, to_write, f) != to_write) {
            ok = false;
            break;
        }
        written += to_write;
        expected++;
        ymodem_put_byte(YMODEM_ACK);
    }

    if (ok) {
        packet_status = ymodem_read_packet(block, &block_len, &block_no);
        if (packet_status == 2) {
            ymodem_put_byte(YMODEM_NAK);
            packet_status = ymodem_read_packet(block, &block_len, &block_no);
        }
        if (packet_status == 2) {
            ymodem_put_byte(YMODEM_ACK);
            ymodem_put_byte(YMODEM_CRC_REQ);
            packet_status = ymodem_read_packet(block, &block_len, &block_no);
            if (packet_status == 1 && block_no == 0) {
                ymodem_put_byte(YMODEM_ACK);
            }
        } else {
            ok = false;
        }
    }

    if (fclose(f) != 0) {
        ok = false;
    }
    free(block);

    if (!ok || written != file_size) {
        remove(resolved);
        shell_error("RECV failed", SHELL_STATUS_IO_ERROR);
        return SHELL_STATUS_IO_ERROR;
    }

    shell_ok();
    return SHELL_STATUS_OK;
}

static shell_status_t shell_send_file(char *args)
{
    if ((shell_exec_flags() & SHELL_EXEC_ALLOW_INTERACTIVE) == 0) {
        shell_error("Not allowed", SHELL_STATUS_NOT_ALLOWED);
        return SHELL_STATUS_NOT_ALLOWED;
    }

    char *cursor = args;
    char *path = next_token(&cursor);
    if (!path || *skip_ws(cursor) != '\0') {
        shell_error("Usage: SEND <path>", SHELL_STATUS_BAD_INPUT);
        return SHELL_STATUS_BAD_INPUT;
    }

    char resolved[STORAGE_PATH_MAX];
    if (!resolve_workspace_arg(path, resolved, sizeof(resolved))) {
        shell_error("Bad path", SHELL_STATUS_BAD_PATH);
        return SHELL_STATUS_BAD_PATH;
    }

    struct stat st;
    if (stat(resolved, &st) != 0) {
        shell_error("Open failed", SHELL_STATUS_NOT_FOUND);
        return SHELL_STATUS_NOT_FOUND;
    }
    if (S_ISDIR(st.st_mode)) {
        shell_error("Is directory", SHELL_STATUS_IS_DIRECTORY);
        return SHELL_STATUS_IS_DIRECTORY;
    }
    if (st.st_size < 0 || (unsigned long)st.st_size > YMODEM_MAX_FILE_SIZE) {
        shell_error("File too large", SHELL_STATUS_BAD_INPUT);
        return SHELL_STATUS_BAD_INPUT;
    }

    size_t file_size = (size_t)st.st_size;
    uint8_t *file_data = NULL;
    if (file_size > 0) {
        file_data = malloc(file_size);
        if (!file_data) {
            shell_error("Out of memory", SHELL_STATUS_IO_ERROR);
            return SHELL_STATUS_IO_ERROR;
        }
    }

    FILE *f = fopen(resolved, "r");
    if (!f) {
        free(file_data);
        shell_error("Open failed", SHELL_STATUS_NOT_FOUND);
        return SHELL_STATUS_NOT_FOUND;
    }
    if (file_size > 0 && fread(file_data, 1, file_size, f) != file_size) {
        free(file_data);
        fclose(f);
        shell_error("Read failed", SHELL_STATUS_IO_ERROR);
        return SHELL_STATUS_IO_ERROR;
    }
    fclose(f);

    shell_status_t status = ymodem_wait_for_crc_request();
    if (status != SHELL_STATUS_OK) {
        free(file_data);
        shell_error(status == SHELL_STATUS_CANCELLED ? "Transfer cancelled" : "YMODEM start failed", status);
        return status;
    }

    uint8_t *block = calloc(1, YMODEM_PACKET_1K);
    if (!block) {
        free(file_data);
        shell_error("Out of memory", SHELL_STATUS_IO_ERROR);
        return SHELL_STATUS_IO_ERROR;
    }

    const char *base = path_basename(path);
    size_t base_len = strlen(base);
    if (base_len == 0 || base_len + 12 >= YMODEM_PACKET_128) {
        free(block);
        free(file_data);
        shell_error("Filename too long", SHELL_STATUS_BAD_INPUT);
        return SHELL_STATUS_BAD_INPUT;
    }
    snprintf((char *)block, YMODEM_PACKET_128, "%s", base);
    snprintf((char *)block + base_len + 1, YMODEM_PACKET_128 - base_len - 1, "%ld", (long)file_size);
    status = ymodem_send_packet(0, block, YMODEM_PACKET_128);

    uint8_t block_no = 1;
    size_t offset = 0;
    while (status == SHELL_STATUS_OK && offset < file_size) {
        size_t n = file_size - offset;
        if (n > YMODEM_PACKET_1K) {
            n = YMODEM_PACKET_1K;
        }
        memcpy(block, file_data + offset, n);
        offset += n;
        if (n < YMODEM_PACKET_1K) {
            memset(block + n, YMODEM_PAD, YMODEM_PACKET_1K - n);
        }
        status = ymodem_send_packet(block_no++, block, YMODEM_PACKET_1K);
    }

    if (status == SHELL_STATUS_OK) {
        ymodem_put_byte(YMODEM_EOT);
        int reply = ymodem_read_byte(YMODEM_TIMEOUT_MS);
        if (reply == YMODEM_NAK) {
            ymodem_put_byte(YMODEM_EOT);
            reply = ymodem_read_byte(YMODEM_TIMEOUT_MS);
        }
        if (reply != YMODEM_ACK) {
            status = SHELL_STATUS_IO_ERROR;
        }
    }

    if (status == SHELL_STATUS_OK) {
        int reply = ymodem_read_byte(YMODEM_TIMEOUT_MS);
        if (reply == YMODEM_CRC_REQ) {
            memset(block, 0, YMODEM_PACKET_128);
            status = ymodem_send_packet(0, block, YMODEM_PACKET_128);
        } else {
            status = SHELL_STATUS_IO_ERROR;
        }
    }

    free(block);
    free(file_data);

    if (status != SHELL_STATUS_OK) {
        shell_error(status == SHELL_STATUS_CANCELLED ? "Transfer cancelled" : "SEND failed", status);
        return status;
    }

    shell_ok();
    return SHELL_STATUS_OK;
}

static uint32_t c3_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static void c3_com_copy_to_exec(void *dst, const uint8_t *src, size_t len)
{
    uint32_t *out = (uint32_t *)dst;
    size_t words = (len + 3u) / 4u;
    for (size_t i = 0; i < words; i++) {
        uint32_t value = 0;
        size_t offset = i * 4u;
        for (size_t j = 0; j < 4u && offset + j < len; j++) {
            value |= (uint32_t)src[offset + j] << (j * 8u);
        }
        out[i] = value;
    }
}

static bool path_has_com_ext(const char *path)
{
    size_t len = path ? strlen(path) : 0;
    return len > 4 && strcasecmp(path + len - 4, ".com") == 0;
}

static void c3_com_stdout_write(const void *data, size_t len)
{
    shell_write_bytes(data, len);
}

static void c3_com_stderr_write(const void *data, size_t len)
{
    shell_write_bytes(data, len);
}

static int c3_com_stdin_read_line(char *buf, size_t len)
{
    return shell_read_line(buf, len);
}

static shell_status_t shell_run_com(char *args)
{
    if ((shell_exec_flags() & SHELL_EXEC_ALLOW_INTERACTIVE) == 0) {
        shell_error("Not allowed", SHELL_STATUS_NOT_ALLOWED);
        return SHELL_STATUS_NOT_ALLOWED;
    }

    char *cursor = args;
    char *path = next_token(&cursor);
    if (!path) {
        shell_error("Usage: RUN /bin/name.com [args...]", SHELL_STATUS_BAD_INPUT);
        return SHELL_STATUS_BAD_INPUT;
    }
    if (!path_has_com_ext(path)) {
        shell_error("Only .com files can run", SHELL_STATUS_BAD_INPUT);
        return SHELL_STATUS_BAD_INPUT;
    }

    const char *argv[C3_COM_ARG_MAX + 1];
    int argc = 0;
    argv[argc++] = path;
    while (argc < C3_COM_ARG_MAX) {
        char *arg = next_token(&cursor);
        if (!arg) {
            break;
        }
        argv[argc++] = arg;
    }
    argv[argc] = NULL;
    if (*skip_ws(cursor) != '\0') {
        shell_error("Too many arguments", SHELL_STATUS_BAD_INPUT);
        return SHELL_STATUS_BAD_INPUT;
    }

    char resolved[STORAGE_PATH_MAX];
    if (!resolve_workspace_arg(path, resolved, sizeof(resolved)) || !workspace_child_path(resolved)) {
        shell_error("Bad path", SHELL_STATUS_BAD_PATH);
        return SHELL_STATUS_BAD_PATH;
    }

    struct stat st;
    if (stat(resolved, &st) != 0) {
        shell_error("Open failed", SHELL_STATUS_NOT_FOUND);
        return SHELL_STATUS_NOT_FOUND;
    }
    if (S_ISDIR(st.st_mode)) {
        shell_error("Is directory", SHELL_STATUS_IS_DIRECTORY);
        return SHELL_STATUS_IS_DIRECTORY;
    }
    if (st.st_size < (off_t)sizeof(c3_com_header_t) ||
        st.st_size > (off_t)(sizeof(c3_com_header_t) + C3_COM_MAX_CODE_SIZE)) {
        shell_error("Bad .com size", SHELL_STATUS_BAD_INPUT);
        return SHELL_STATUS_BAD_INPUT;
    }

    FILE *f = fopen(resolved, "rb");
    if (!f) {
        shell_error("Open failed", SHELL_STATUS_NOT_FOUND);
        return SHELL_STATUS_NOT_FOUND;
    }

    c3_com_header_t header;
    bool ok = fread(&header, 1, sizeof(header), f) == sizeof(header);
    if (!ok ||
        header.magic[0] != C3_COM_MAGIC_0 ||
        header.magic[1] != C3_COM_MAGIC_1 ||
        header.magic[2] != C3_COM_MAGIC_2 ||
        header.magic[3] != C3_COM_MAGIC_3 ||
        header.header_size < sizeof(c3_com_header_t) ||
        header.abi_version != C3_COM_ABI_VERSION ||
        header.target != C3_COM_TARGET_ESP32C3_RV32 ||
        header.code_size == 0 ||
        header.code_size > C3_COM_MAX_CODE_SIZE ||
        header.code_offset < header.header_size ||
        header.entry_offset >= header.code_size ||
        (header.entry_offset & 0x3u) != 0 ||
        (uint64_t)header.code_offset + header.code_size > (uint64_t)st.st_size) {
        fclose(f);
        shell_error("Invalid .com header", SHELL_STATUS_BAD_INPUT);
        return SHELL_STATUS_BAD_INPUT;
    }

    uint8_t *image = malloc(header.code_size);
    if (!image) {
        fclose(f);
        shell_error("Out of memory", SHELL_STATUS_IO_ERROR);
        return SHELL_STATUS_IO_ERROR;
    }

    if (fseek(f, (long)header.code_offset, SEEK_SET) != 0 ||
        fread(image, 1, header.code_size, f) != header.code_size ||
        c3_crc32(image, header.code_size) != header.code_crc32) {
        free(image);
        fclose(f);
        shell_error("Invalid .com image", SHELL_STATUS_BAD_INPUT);
        return SHELL_STATUS_BAD_INPUT;
    }
    fclose(f);

    void *code = heap_caps_aligned_alloc(4, (header.code_size + 3u) & ~3u, MALLOC_CAP_EXEC);
    if (!code) {
        free(image);
        shell_error("No exec memory", SHELL_STATUS_IO_ERROR);
        return SHELL_STATUS_IO_ERROR;
    }
    c3_com_copy_to_exec(code, image, header.code_size);
    free(image);

    __builtin___clear_cache((char *)code, (char *)code + header.code_size);

    c3_com_api_t api = {
        .abi_version = C3_COM_ABI_VERSION,
        .argc = argc,
        .argv = argv,
        .stdin_read_line = c3_com_stdin_read_line,
        .stdout_write = c3_com_stdout_write,
        .stderr_write = c3_com_stderr_write,
    };
    c3_com_entry_fn entry = (c3_com_entry_fn)(void *)(code + header.entry_offset);
    int rc = entry(&api, argc, argv);
    heap_caps_free(code);

    char line[40];
    snprintf(line, sizeof(line), "\r\nEXIT %d\r\n", rc);
    shell_puts(line);
    shell_set_status(rc == 0 ? SHELL_STATUS_OK : SHELL_STATUS_IO_ERROR);
    return rc == 0 ? SHELL_STATUS_OK : SHELL_STATUS_IO_ERROR;
}

static void shell_dispatch_command(shell_command_t command, char *arg)
{
    switch (command) {
    case SHELL_COMMAND_HELP:
        shell_help();
        break;
    case SHELL_COMMAND_DF:
        shell_df(arg);
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
    case SHELL_COMMAND_RECV:
        shell_recv_file(arg);
        break;
    case SHELL_COMMAND_SEND:
        shell_send_file(arg);
        break;
    case SHELL_COMMAND_RUN:
        shell_run_com(arg);
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
    } else if (strcasecmp(name, "DF") == 0) {
        *out = SHELL_COMMAND_DF;
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
    } else if (strcasecmp(name, "RECV") == 0) {
        *out = SHELL_COMMAND_RECV;
    } else if (strcasecmp(name, "SEND") == 0) {
        *out = SHELL_COMMAND_SEND;
    } else if (strcasecmp(name, "RUN") == 0) {
        *out = SHELL_COMMAND_RUN;
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
