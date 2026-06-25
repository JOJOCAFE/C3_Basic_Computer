#include "storage.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_littlefs.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "storage";

static const char *const kWorkspaceDirs[] = {
    "basic",
    "asm",
    "bin",
    "config",
    "data",
    "temp",
    "BASIC",
    "ASM",
    "CONFIG",
    "TEMP",
};

static const size_t kWorkspaceDirCount = sizeof(kWorkspaceDirs) / sizeof(kWorkspaceDirs[0]);

static esp_err_t storage_mount_workspace(void);
static esp_err_t storage_mount_system(void);
static esp_err_t storage_prepare_workspace_layout(void);

static esp_err_t ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return ESP_OK;
        }
        return ESP_FAIL;
    }

    if (mkdir(path, 0775) == 0 || errno == EEXIST) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

static esp_err_t storage_mount(const char *base_path, const char *partition_label, bool format_if_mount_failed, bool read_only)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = base_path,
        .partition_label = partition_label,
        .format_if_mount_failed = format_if_mount_failed,
        .dont_mount = false,
        .read_only = read_only,
    };

    return esp_vfs_littlefs_register(&conf);
}

static esp_err_t storage_unmount_workspace(void)
{
    esp_err_t err = esp_vfs_littlefs_unregister(STORAGE_WORKSPACE_PARTITION_LABEL);
    if (err == ESP_ERR_NOT_FOUND || err == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    return err;
}

static esp_err_t storage_mount_system(void)
{
    return storage_mount(STORAGE_SYSTEM_MOUNT_POINT, STORAGE_SYSTEM_PARTITION_LABEL, true, true);
}

static esp_err_t storage_mount_workspace(void)
{
    return storage_mount(STORAGE_WORKSPACE_MOUNT_POINT, STORAGE_WORKSPACE_PARTITION_LABEL, true, false);
}

static esp_err_t storage_prepare_workspace_layout(void)
{
    for (size_t i = 0; i < kWorkspaceDirCount; ++i) {
        char path[64];
        if (snprintf(path, sizeof(path), "%s/%s", STORAGE_WORKSPACE_MOUNT_POINT, kWorkspaceDirs[i]) >= (int)sizeof(path)) {
            return ESP_ERR_INVALID_SIZE;
        }
        if (ensure_dir(path) != ESP_OK) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t storage_init(void)
{
    esp_err_t err = storage_mount_system();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to mount system filesystem: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "system FS at %s", STORAGE_SYSTEM_MOUNT_POINT);
    }

    err = storage_mount_workspace();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to mount workspace filesystem: %s", esp_err_to_name(err));
        return err;
    }

    if (storage_prepare_workspace_layout() != ESP_OK) {
        ESP_LOGE(TAG, "failed to create workspace directories");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "workspace FS at %s", STORAGE_WORKSPACE_MOUNT_POINT);
    return ESP_OK;
}

esp_err_t storage_renew_workspace(void)
{
    esp_err_t err = storage_unmount_workspace();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to unmount workspace filesystem: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_littlefs_format(STORAGE_WORKSPACE_PARTITION_LABEL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to format workspace partition: %s", esp_err_to_name(err));
        return err;
    }

    err = storage_mount_workspace();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to remount workspace filesystem: %s", esp_err_to_name(err));
        return err;
    }

    if (storage_prepare_workspace_layout() != ESP_OK) {
        ESP_LOGE(TAG, "failed to recreate workspace directories after renew");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "workspace renew complete");
    return ESP_OK;
}

bool storage_resolve_path(const char *input, char *out, size_t out_size)
{
    if (!input || !out || out_size == 0) {
        return false;
    }

    while (isspace((unsigned char)*input)) {
        input++;
    }

    if (*input == '\0') {
        return false;
    }

    if (strstr(input, "..") != NULL) {
        return false;
    }

    if (*input == '/') {
        return snprintf(out, out_size, "%s%s", STORAGE_WORKSPACE_MOUNT_POINT, input) < (int)out_size;
    }

    if (strchr(input, '/') == NULL && strchr(input, '.') == NULL) {
        return snprintf(out, out_size, "%s/basic/%s.bas", STORAGE_WORKSPACE_MOUNT_POINT, input) < (int)out_size;
    }

    return snprintf(out, out_size, "%s/%s", STORAGE_WORKSPACE_MOUNT_POINT, input) < (int)out_size;
}
