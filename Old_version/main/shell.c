#include "shell.h"

#include "basic.h"
#include "input.h"
#include "storage.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define SHELL_BUF 256
#define FILE_IO_BUF 128

static basic_program_t g_program;
static char g_workspace_cwd[STORAGE_PATH_MAX];

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

static void input_write_str(void *ctx, const char *text)
{
    (void)ctx;
    input_write(text);
}

static int input_read_line_adapter(void *ctx, char *buf, size_t len)
{
    (void)ctx;
    return input_read_line(buf, len);
}

static void shell_puts(const char *text)
{
    input_write(text);
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

static void shell_reset_workspace_cwd(void)
{
    snprintf(g_workspace_cwd, sizeof(g_workspace_cwd), "/");
}

static void shell_help(void)
{
    shell_puts(
        "HELP PWD DIR LS CD MKDIR CAT WRITE RM COPY CP MOVE MV\r\n"
        "LOAD SAVE DELETE NEW RUN LIST RENEW\r\n"
        "Type numbered BASIC lines to store them.\r\n"
        "Math: ABS INT ROUND MIN MAX SQRT SIN COS TAN ASIN ACOS ATAN ATAN2 LOG EXP RND PI E DEG RAD\r\n"
        "Example: 10 PRINT SIN(30)\r\n");
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

    if (input_read_line(line, sizeof(line)) <= 0) {
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
    shell_puts("WARNING.\r\n");
    shell_puts("This operation removes\r\n");
    shell_puts("all user programs and data.\r\n");
    shell_puts("The system software\r\n");
    shell_puts("will remain unchanged.\r\n");

    if (!shell_confirm("Do you understand? (Y/N)")) {
        shell_puts("CANCELLED.\r\n");
        return;
    }

    if (!shell_confirm("Ready to continue? (Y/N)")) {
        shell_puts("CANCELLED.\r\n");
        return;
    }

    if (storage_renew_workspace() != ESP_OK) {
        shell_puts("RENEW failed\r\n");
        return;
    }

    shell_puts("OK\r\n");
}

static bool is_basic_immediate_command(const char *line)
{
    const char *trimmed = skip_ws(line);
    if (trimmed == NULL || *trimmed == '\0') {
        return false;
    }

    if (isdigit((unsigned char)*trimmed)) {
        return false;
    }

    if (!isalpha((unsigned char)*trimmed)) {
        return false;
    }

    char token[16];
    size_t tok_len = 0;
    const char *cursor = trimmed;
    while (isalpha((unsigned char)*cursor) && tok_len + 1 < sizeof(token)) {
        token[tok_len++] = (char)toupper((unsigned char)*cursor++);
        }
    token[tok_len] = '\0';

    if (tok_len == 1) {
        cursor = skip_ws(cursor);
        return *cursor == '=';
    }

    if (strcmp(token, "REM") == 0 ||
        strcmp(token, "PRINT") == 0 ||
        strcmp(token, "LET") == 0 ||
        strcmp(token, "INPUT") == 0 ||
        strcmp(token, "IF") == 0 ||
        strcmp(token, "FOR") == 0 ||
        strcmp(token, "NEXT") == 0 ||
        strcmp(token, "GOTO") == 0 ||
        strcmp(token, "GOSUB") == 0 ||
        strcmp(token, "RETURN") == 0 ||
        strcmp(token, "END") == 0) {
        return true;
    }

    return false;
}

static void shell_list_dir(const char *path)
{
    char resolved[160];
    if (path && strstr(path, "..") != NULL) {
        shell_puts("Bad path\r\n");
        return;
    }

    if (!path || *path == '\0') {
        snprintf(resolved, sizeof(resolved), "%s", STORAGE_MOUNT_POINT);
    } else if (*path == '/') {
        if (snprintf(resolved, sizeof(resolved), "%s%s", STORAGE_MOUNT_POINT, path) >= (int)sizeof(resolved)) {
            shell_puts("Bad path\r\n");
            return;
        }
    } else {
        if (snprintf(resolved, sizeof(resolved), "%s/%s", STORAGE_MOUNT_POINT, path) >= (int)sizeof(resolved)) {
            shell_puts("Bad path\r\n");
            return;
        }
    }

    DIR *dir = opendir(resolved);
    if (!dir) {
        shell_puts("Cannot open directory\r\n");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        shell_puts(entry->d_name);
        shell_puts("\r\n");
    }
    closedir(dir);
}

static void shell_ls(const char *path)
{
    char resolved[STORAGE_PATH_MAX];
    const char *arg = skip_ws(path);
    if (!storage_resolve_workspace_path(g_workspace_cwd, arg, resolved, sizeof(resolved))) {
        shell_puts("Bad path\r\n");
        return;
    }

    DIR *dir = opendir(resolved);
    if (!dir) {
        shell_puts("Cannot open directory\r\n");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        shell_puts(entry->d_name);
        shell_puts("\r\n");
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
        shell_puts("Bad path\r\n");
        return;
    }

    if (!path_is_dir(resolved)) {
        shell_puts("Not directory\r\n");
        return;
    }

    snprintf(g_workspace_cwd, sizeof(g_workspace_cwd), "%s", normalized);
    shell_puts("OK\r\n");
}

static void shell_mkdir(const char *path)
{
    char resolved[STORAGE_PATH_MAX];
    if (!resolve_workspace_arg(path, resolved, sizeof(resolved))) {
        shell_puts("Bad path\r\n");
        return;
    }

    struct stat st;
    if (stat(resolved, &st) == 0) {
        shell_puts("Exists\r\n");
        return;
    }

    if (mkdir(resolved, 0775) != 0) {
        shell_puts("MKDIR failed\r\n");
        return;
    }

    shell_puts("OK\r\n");
}

static void shell_cat(const char *path)
{
    char resolved[STORAGE_PATH_MAX];
    if (!resolve_workspace_arg(path, resolved, sizeof(resolved))) {
        shell_puts("Bad path\r\n");
        return;
    }

    if (path_is_dir(resolved)) {
        shell_puts("Is directory\r\n");
        return;
    }

    FILE *f = fopen(resolved, "r");
    if (!f) {
        shell_puts("Open failed\r\n");
        return;
    }

    char buf[FILE_IO_BUF];
    size_t n;
    char last = '\0';
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        input_write_bytes(buf, n);
        last = buf[n - 1];
    }

    if (ferror(f)) {
        shell_puts("\r\nRead failed\r\n");
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
    if (!path || !text || *text == '\0') {
        shell_puts("Bad input\r\n");
        return;
    }

    char resolved[STORAGE_PATH_MAX];
    if (!resolve_workspace_arg(path, resolved, sizeof(resolved))) {
        shell_puts("Bad path\r\n");
        return;
    }

    struct stat st;
    if (stat(resolved, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            shell_puts("Is directory\r\n");
            return;
        }
        if (!force) {
            shell_puts("Exists\r\n");
            return;
        }
    }

    FILE *f = fopen(resolved, "w");
    if (!f) {
        shell_puts("WRITE failed\r\n");
        return;
    }

    size_t len = strlen(text);
    bool ok = fwrite(text, 1, len, f) == len;
    if (fclose(f) != 0) {
        ok = false;
    }
    if (!ok) {
        shell_puts("WRITE failed\r\n");
        return;
    }

    shell_puts("OK\r\n");
}

static void shell_rm(const char *path)
{
    char resolved[STORAGE_PATH_MAX];
    if (!resolve_workspace_arg(path, resolved, sizeof(resolved))) {
        shell_puts("Bad path\r\n");
        return;
    }

    if (path_is_dir(resolved)) {
        shell_puts("Is directory\r\n");
        return;
    }

    if (remove(resolved) != 0) {
        shell_puts("RM failed\r\n");
        return;
    }

    shell_puts("OK\r\n");
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
        shell_puts("Bad input\r\n");
        return false;
    }

    if (!resolve_workspace_arg(src, src_path, src_size) ||
        !resolve_workspace_arg(dst, dst_path, dst_size)) {
        shell_puts("Bad path\r\n");
        return false;
    }

    struct stat st;
    if (stat(src_path, &st) != 0) {
        shell_puts("Open failed\r\n");
        return false;
    }

    if (S_ISDIR(st.st_mode) || path_is_dir(dst_path)) {
        shell_puts("Is directory\r\n");
        return false;
    }

    if (stat(dst_path, &st) == 0) {
        shell_puts("Exists\r\n");
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
        shell_puts("Open failed\r\n");
        return;
    }

    FILE *dst = fopen(dst_path, "w");
    if (!dst) {
        fclose(src);
        shell_puts("COPY failed\r\n");
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
        shell_puts("COPY failed\r\n");
        return;
    }

    shell_puts("OK\r\n");
}

static void shell_move(char *args)
{
    char src_path[STORAGE_PATH_MAX];
    char dst_path[STORAGE_PATH_MAX];
    if (!resolve_file_pair(args, src_path, sizeof(src_path), dst_path, sizeof(dst_path))) {
        return;
    }

    if (rename(src_path, dst_path) != 0) {
        shell_puts("MOVE failed\r\n");
        return;
    }

    shell_puts("OK\r\n");
}

static void shell_load(const char *name)
{
    char path[160];
    if (!storage_resolve_path(name, path, sizeof(path))) {
        shell_puts("Bad filename\r\n");
        return;
    }

    esp_err_t err = basic_load_file(path, &g_program);
    if (err != ESP_OK) {
        shell_puts("LOAD failed\r\n");
        return;
    }
    shell_puts("OK\r\n");
}

static void shell_save(const char *name)
{
    char path[160];
    if (!storage_resolve_path(name, path, sizeof(path))) {
        shell_puts("Bad filename\r\n");
        return;
    }

    esp_err_t err = basic_save_file(path, &g_program);
    if (err != ESP_OK) {
        shell_puts("SAVE failed\r\n");
        return;
    }
    shell_puts("OK\r\n");
}

static void shell_delete(const char *name)
{
    char path[160];
    if (!storage_resolve_path(name, path, sizeof(path))) {
        shell_puts("Bad filename\r\n");
        return;
    }

    if (remove(path) != 0) {
        shell_puts("DELETE failed\r\n");
        return;
    }
    shell_puts("OK\r\n");
}

static void shell_execute_line(const char *line)
{
    basic_io_t io = {
        .read_line = input_read_line_adapter,
        .write = input_write_str,
        .ctx = NULL,
    };

    const char *trimmed = line;
    while (*trimmed && isspace((unsigned char)*trimmed)) {
        trimmed++;
    }

    if (isdigit((unsigned char)trimmed[0])) {
        if (basic_store_line(&g_program, trimmed)) {
            shell_puts("OK\r\n");
        } else {
            shell_puts("LINE ERROR\r\n");
        }
        return;
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

    if (strcasecmp(command, "HELP") == 0) {
        shell_help();
    } else if (strcasecmp(command, "PWD") == 0) {
        shell_pwd();
    } else if (strcasecmp(command, "DIR") == 0) {
        shell_list_dir(arg);
    } else if (strcasecmp(command, "LS") == 0) {
        shell_ls(arg);
    } else if (strcasecmp(command, "CD") == 0) {
        shell_cd(arg);
    } else if (strcasecmp(command, "MKDIR") == 0) {
        shell_mkdir(arg);
    } else if (strcasecmp(command, "CAT") == 0) {
        shell_cat(arg);
    } else if (strcasecmp(command, "WRITE") == 0) {
        shell_write_file(arg);
    } else if (strcasecmp(command, "RM") == 0) {
        shell_rm(arg);
    } else if (strcasecmp(command, "COPY") == 0 || strcasecmp(command, "CP") == 0) {
        shell_copy(arg);
    } else if (strcasecmp(command, "MOVE") == 0 || strcasecmp(command, "MV") == 0) {
        shell_move(arg);
    } else if (strcasecmp(command, "LIST") == 0) {
        basic_list(&g_program, input_write_str, NULL);
    } else if (strcasecmp(command, "LOAD") == 0) {
        shell_load(arg);
    } else if (strcasecmp(command, "SAVE") == 0) {
        shell_save(arg);
    } else if (strcasecmp(command, "DELETE") == 0) {
        shell_delete(arg);
    } else if (strcasecmp(command, "NEW") == 0) {
        basic_clear(&g_program);
        shell_puts("OK\r\n");
    } else if (strcasecmp(command, "RENEW") == 0) {
        shell_renew_workspace();
    } else if (strcasecmp(command, "RUN") == 0) {
        esp_err_t err = basic_run(&g_program, &io);
        if (err == ESP_OK) {
            shell_puts("OK\r\n");
        }
    } else if (command[0] != '\0') {
        if (!is_basic_immediate_command(line)) {
            shell_puts("UNKNOWN COMMAND\r\n");
            return;
        }
        if (basic_execute_immediate(&g_program, line, &io) != ESP_OK) {
            shell_puts("UNKNOWN COMMAND\r\n");
        }
    }
}

void shell_run(void)
{
    ESP_ERROR_CHECK(input_init());

    basic_init(&g_program);
    shell_reset_workspace_cwd();
    shell_banner();

    char line[SHELL_BUF];
    while (1) {
        shell_prompt();
        if (input_read_line(line, sizeof(line)) <= 0) {
            continue;
        }
        shell_execute_line(line);
    }
}
