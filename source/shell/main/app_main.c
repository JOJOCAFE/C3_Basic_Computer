#include "shell.h"
#include "storage.h"

#include "esp_log.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static const char *TAG = "c3shell";

typedef struct {
    char data[256];
    size_t used;
} api_capture_t;

static void api_capture_reset(api_capture_t *cap)
{
    cap->used = 0;
    cap->data[0] = '\0';
}

static void api_capture_write(void *ctx, const void *data, size_t len)
{
    api_capture_t *cap = (api_capture_t *)ctx;
    size_t room = sizeof(cap->data) - cap->used;

    if (room <= 1) {
        return;
    }
    if (len > room - 1) {
        len = room - 1;
    }

    memcpy(cap->data + cap->used, data, len);
    cap->used += len;
    cap->data[cap->used] = '\0';
}

static bool shell_api_self_test(void)
{
    shell_command_t command;
    api_capture_t cap;
    shell_exec_io_t io = {
        .write = api_capture_write,
        .read_line = NULL,
        .ctx = &cap,
        .flags = 0,
    };

    if (shell_command_from_name("LS", &command) != SHELL_STATUS_OK ||
        command != SHELL_COMMAND_LS) {
        ESP_LOGE(TAG, "shell API command lookup failed");
        return false;
    }

    if (shell_command_from_name("PRINT", &command) != SHELL_STATUS_UNKNOWN_COMMAND) {
        ESP_LOGE(TAG, "shell API exposed BASIC command");
        return false;
    }

    if (strcmp(shell_status_name(SHELL_STATUS_NOT_ALLOWED), "NOT_ALLOWED") != 0) {
        ESP_LOGE(TAG, "shell API status name failed");
        return false;
    }

    if (shell_exec_line(NULL, &io) != SHELL_STATUS_BAD_INPUT ||
        shell_exec_line("", &io) != SHELL_STATUS_EMPTY) {
        ESP_LOGE(TAG, "shell API input validation failed");
        return false;
    }

    shell_reset();
    api_capture_reset(&cap);
    if (shell_exec_line("PWD", &io) != SHELL_STATUS_OK ||
        strcmp(cap.data, "/\r\n") != 0) {
        ESP_LOGE(TAG, "shell API PWD dispatch failed: %s", cap.data);
        return false;
    }

    api_capture_reset(&cap);
    if (shell_exec_command(SHELL_COMMAND_HELP, NULL, &io) != SHELL_STATUS_OK ||
        strstr(cap.data, "HELP PWD LS") == NULL) {
        ESP_LOGE(TAG, "shell API HELP dispatch failed");
        return false;
    }

    api_capture_reset(&cap);
    if (shell_exec_line("RENEW", &io) != SHELL_STATUS_NOT_ALLOWED ||
        strstr(cap.data, "Not allowed") == NULL) {
        ESP_LOGE(TAG, "shell API RENEW guard failed");
        return false;
    }

    return true;
}

void app_main(void)
{
    ESP_LOGI(TAG, "Booting standalone C3 shell");

    if (storage_init() != ESP_OK) {
        ESP_LOGE(TAG, "storage init failed");
        return;
    }

    if (!shell_api_self_test()) {
        return;
    }

    shell_run();
}
//Keep Going.
