#include "storage.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "storage";
static bool s_workspace_ready = false;
static wl_handle_t s_system_wl = WL_INVALID_HANDLE;
static wl_handle_t s_workspace_wl = WL_INVALID_HANDLE;

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

static void trim_path_copy(const char *input, char *out, size_t out_size)
{
    if (out_size == 0) {
        return;
    }

    out[0] = '\0';
    if (!input) {
        return;
    }

    while (isspace((unsigned char)*input)) {
        input++;
    }

    size_t len = strlen(input);
    while (len > 0 && isspace((unsigned char)input[len - 1])) {
        len--;
    }

    if (len >= out_size) {
        len = out_size - 1;
    }

    memcpy(out, input, len);
    out[len] = '\0';
}

static bool workspace_path_pop(char *path)
{
    if (strcmp(path, "/") == 0) {
        return true;
    }

    char *last_sep = strrchr(path, '/');
    if (!last_sep) {
        return false;
    }

    if (last_sep == path) {
        path[1] = '\0';
    } else {
        *last_sep = '\0';
    }
    return true;
}

static bool workspace_path_append(char *path, size_t path_size, const char *component)
{
    if (!component || component[0] == '\0' || strcmp(component, ".") == 0) {
        return true;
    }

    if (strcmp(component, "..") == 0) {
        return workspace_path_pop(path);
    }

    if (strchr(component, '\\') != NULL) {
        return false;
    }

    int written;
    size_t available;
    if (strcmp(path, "/") == 0) {
        available = path_size;
        written = snprintf(path, available, "/%s", component);
    } else {
        size_t len = strlen(path);
        if (len >= path_size) {
            return false;
        }
        available = path_size - len;
        written = snprintf(path + len, available, "/%s", component);
    }

    return written > 0 && (size_t)written < available;
}

static bool workspace_path_apply(char *path, size_t path_size, const char *input)
{
    char temp[STORAGE_PATH_MAX];
    trim_path_copy(input, temp, sizeof(temp));

    char *saveptr = NULL;
    char *token = strtok_r(temp, "/", &saveptr);
    while (token) {
        if (!workspace_path_append(path, path_size, token)) {
            return false;
        }
        token = strtok_r(NULL, "/", &saveptr);
    }

    return true;
}

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

static esp_err_t mount_fatfs(const char *base_path, const char *partition_label, bool format_if_mount_failed, wl_handle_t *handle)
{
    *handle = WL_INVALID_HANDLE;

    esp_vfs_fat_mount_config_t config = {
        .format_if_mount_failed = format_if_mount_failed,
        .max_files = 8,
        .allocation_unit_size = 4096,
    };

    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(base_path, partition_label, &config, handle);
    if (err != ESP_OK) {
        *handle = WL_INVALID_HANDLE;
    }
    return err;
}

static esp_err_t storage_mount_system(void)
{
    return mount_fatfs(STORAGE_SYSTEM_MOUNT_POINT, STORAGE_SYSTEM_PARTITION_LABEL, true, &s_system_wl);
}

static esp_err_t storage_mount_workspace(bool format_if_mount_failed)
{
    return mount_fatfs(STORAGE_WORKSPACE_MOUNT_POINT, STORAGE_WORKSPACE_PARTITION_LABEL, format_if_mount_failed, &s_workspace_wl);
}

static esp_err_t storage_unmount_workspace(void)
{
    if (s_workspace_wl == WL_INVALID_HANDLE) {
        return ESP_OK;
    }

    esp_err_t err = esp_vfs_fat_spiflash_unmount_rw_wl(STORAGE_WORKSPACE_MOUNT_POINT, s_workspace_wl);
    s_workspace_wl = WL_INVALID_HANDLE;
    if (err == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    return err;
}

static esp_err_t storage_prepare_workspace_layout(void)
{
    for (size_t i = 0; i < sizeof(kWorkspaceDirs) / sizeof(kWorkspaceDirs[0]); ++i) {
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
    s_workspace_ready = false;

    esp_err_t err = storage_mount_system();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to mount system filesystem: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "system FS at %s", STORAGE_SYSTEM_MOUNT_POINT);
    }

    err = storage_mount_workspace(false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to mount workspace filesystem: %s", esp_err_to_name(err));
        ESP_LOGW(TAG, "shell will start; run RENEW to build workspace_fs");
        return ESP_OK;
    }

    if (storage_prepare_workspace_layout() != ESP_OK) {
        ESP_LOGE(TAG, "failed to create workspace directories");
        ESP_LOGW(TAG, "shell will start; run RENEW to rebuild workspace_fs");
        return ESP_OK;
    }

    s_workspace_ready = true;
    ESP_LOGI(TAG, "workspace FS at %s", STORAGE_WORKSPACE_MOUNT_POINT);
    return ESP_OK;
}

esp_err_t storage_renew_workspace(void)
{
    s_workspace_ready = false;

    ESP_LOGI(TAG, "renew workspace: unmounting %s", STORAGE_WORKSPACE_PARTITION_LABEL);
    esp_err_t err = storage_unmount_workspace();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to unmount workspace filesystem: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "renew workspace: unmounted %s", STORAGE_WORKSPACE_PARTITION_LABEL);

    ESP_LOGI(TAG, "renew workspace: formatting %s", STORAGE_WORKSPACE_PARTITION_LABEL);
    err = esp_vfs_fat_spiflash_format_rw_wl(STORAGE_WORKSPACE_MOUNT_POINT, STORAGE_WORKSPACE_PARTITION_LABEL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to format workspace partition: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "renew workspace: formatted %s", STORAGE_WORKSPACE_PARTITION_LABEL);

    ESP_LOGI(TAG, "renew workspace: remounting %s at %s", STORAGE_WORKSPACE_PARTITION_LABEL, STORAGE_WORKSPACE_MOUNT_POINT);
    err = storage_mount_workspace(false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to remount workspace filesystem: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "renew workspace: remounted %s at %s", STORAGE_WORKSPACE_PARTITION_LABEL, STORAGE_WORKSPACE_MOUNT_POINT);

    ESP_LOGI(TAG, "renew workspace: recreating workspace layout");
    if (storage_prepare_workspace_layout() != ESP_OK) {
        ESP_LOGE(TAG, "failed to recreate workspace directories after renew");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "renew workspace: workspace layout ready");

    s_workspace_ready = true;
    ESP_LOGI(TAG, "workspace renew complete");
    return ESP_OK;
}

bool storage_workspace_ready(void)
{
    return s_workspace_ready;
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

bool storage_normalize_workspace_path(const char *cwd, const char *input, char *out, size_t out_size)
{
    if (!cwd || !out || out_size < 2 || cwd[0] != '/') {
        return false;
    }

    char trimmed_input[STORAGE_PATH_MAX];
    trim_path_copy(input, trimmed_input, sizeof(trimmed_input));

    out[0] = '/';
    out[1] = '\0';

    if (!workspace_path_apply(out, out_size, cwd + 1)) {
        return false;
    }

    if (trimmed_input[0] == '\0') {
        return true;
    }

    if (trimmed_input[0] == '/') {
        out[0] = '/';
        out[1] = '\0';
        return workspace_path_apply(out, out_size, trimmed_input + 1);
    }

    return workspace_path_apply(out, out_size, trimmed_input);
}

bool storage_resolve_workspace_path(const char *cwd, const char *input, char *out, size_t out_size)
{
    if (!out || out_size == 0) {
        return false;
    }

    char relative[STORAGE_PATH_MAX];
    if (!storage_normalize_workspace_path(cwd, input, relative, sizeof(relative))) {
        return false;
    }

    return snprintf(out, out_size, "%s%s", STORAGE_WORKSPACE_MOUNT_POINT, relative) < (int)out_size;
}
