#include "shell.h"

#include "basic.h"
#include "storage.h"

#include "driver/usb_serial_jtag.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define SHELL_BUF 256

static basic_program_t g_program;

static const char *skip_ws(const char *s)
{
    while (s && *s && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

static void uart_write_str(void *ctx, const char *text)
{
    (void)ctx;
    usb_serial_jtag_write_bytes(text, strlen(text), portMAX_DELAY);
}

static int uart_read_line(void *ctx, char *buf, size_t len)
{
    (void)ctx;
    size_t idx = 0;
    while (idx + 1 < len) {
        char ch;
        int n = usb_serial_jtag_read_bytes(&ch, 1, portMAX_DELAY);
        if (n <= 0) {
            continue;
        }

        if (ch == '\r' || ch == '\n') {
            usb_serial_jtag_write_bytes("\r\n", 2, portMAX_DELAY);
            break;
        }

        if (ch == '\b' || ch == 0x7f) {
            if (idx > 0) {
                idx--;
                usb_serial_jtag_write_bytes("\b \b", 3, portMAX_DELAY);
            }
            continue;
        }

        buf[idx++] = ch;
        usb_serial_jtag_write_bytes(&ch, 1, portMAX_DELAY);
    }

    buf[idx] = '\0';
    return (int)idx;
}

static void shell_puts(const char *text)
{
    usb_serial_jtag_write_bytes(text, strlen(text), portMAX_DELAY);
}

static void shell_banner(void)
{
    shell_puts("\r\nC3 BASIC COMPUTER\r\nVersion 0.1\r\n\r\nREADY.\r\n");
}

static void shell_prompt(void)
{
    shell_puts("> ");
}

static void shell_help(void)
{
    shell_puts(
        "HELP DIR LOAD SAVE DELETE NEW RUN LIST RENEW\r\n"
        "Type numbered BASIC lines to store them.\r\n"
        "Math: ABS INT ROUND MIN MAX SQRT SIN COS TAN ASIN ACOS ATAN ATAN2 LOG EXP RND PI E DEG RAD\r\n"
        "Example: 10 PRINT SIN(30)\r\n");
}

static bool shell_confirm(const char *prompt)
{
    char line[SHELL_BUF];
    shell_puts(prompt);
    shell_puts("\r\n");

    if (uart_read_line(NULL, line, sizeof(line)) <= 0) {
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
        .read_line = uart_read_line,
        .write = uart_write_str,
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
    } else if (strcasecmp(command, "DIR") == 0) {
        shell_list_dir(arg);
    } else if (strcasecmp(command, "LIST") == 0) {
        basic_list(&g_program, uart_write_str, NULL);
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
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&cfg));

    basic_init(&g_program);
    shell_banner();

    char line[SHELL_BUF];
    while (1) {
        shell_prompt();
        if (uart_read_line(NULL, line, sizeof(line)) <= 0) {
            continue;
        }
        shell_execute_line(line);
    }
}
