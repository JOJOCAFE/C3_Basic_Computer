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
static bool s_workspace_ready = false;

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

static bool workspace_path_component_valid(const char *component)
{
    const unsigned char *scan = (const unsigned char *)component;
    while (*scan) {
        unsigned char c = *scan++;
        if (c < 0x20 || c == 0x7f) {
            return false;
        }
        if (c < 0x80) {
            continue;
        }

        if (c >= 0xc2 && c <= 0xdf) {
            if (strlen((const char *)scan) < 1) {
                return false;
            }
            if ((*scan & 0xc0) != 0x80) {
                return false;
            }
            scan++;
        } else if (c == 0xe0) {
            if (strlen((const char *)scan) < 2) {
                return false;
            }
            if (scan[0] < 0xa0 || scan[0] > 0xbf || (scan[1] & 0xc0) != 0x80) {
                return false;
            }
            scan += 2;
        } else if ((c >= 0xe1 && c <= 0xec) || (c >= 0xee && c <= 0xef)) {
            if (strlen((const char *)scan) < 2) {
                return false;
            }
            if ((scan[0] & 0xc0) != 0x80 || (scan[1] & 0xc0) != 0x80) {
                return false;
            }
            scan += 2;
        } else if (c == 0xed) {
            if (strlen((const char *)scan) < 2) {
                return false;
            }
            if (scan[0] < 0x80 || scan[0] > 0x9f || (scan[1] & 0xc0) != 0x80) {
                return false;
            }
            scan += 2;
        } else if (c == 0xf0) {
            if (strlen((const char *)scan) < 3) {
                return false;
            }
            if (scan[0] < 0x90 || scan[0] > 0xbf ||
                (scan[1] & 0xc0) != 0x80 || (scan[2] & 0xc0) != 0x80) {
                return false;
            }
            scan += 3;
        } else if (c >= 0xf1 && c <= 0xf3) {
            if (strlen((const char *)scan) < 3) {
                return false;
            }
            if ((scan[0] & 0xc0) != 0x80 || (scan[1] & 0xc0) != 0x80 ||
                (scan[2] & 0xc0) != 0x80) {
                return false;
            }
            scan += 3;
        } else if (c == 0xf4) {
            if (strlen((const char *)scan) < 3) {
                return false;
            }
            if (scan[0] < 0x80 || scan[0] > 0x8f ||
                (scan[1] & 0xc0) != 0x80 || (scan[2] & 0xc0) != 0x80) {
                return false;
            }
            scan += 3;
        } else {
            return false;
        }
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

    if (!workspace_path_component_valid(component) || strchr(component, '\\') != NULL) {
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
    return storage_mount(STORAGE_SYSTEM_MOUNT_POINT, STORAGE_SYSTEM_PARTITION_LABEL, false, true);
}

static esp_err_t storage_mount_workspace(void)
{
    return storage_mount(STORAGE_WORKSPACE_MOUNT_POINT, STORAGE_WORKSPACE_PARTITION_LABEL, false, false);
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
    s_workspace_ready = false;

    esp_err_t err = storage_mount_system();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to mount system filesystem: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "system FS at %s", STORAGE_SYSTEM_MOUNT_POINT);
    }

    err = storage_mount_workspace();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to mount workspace filesystem: %s", esp_err_to_name(err));
        ESP_LOGW(TAG, "shell will start from protected system path; use RENEW to rebuild workspace");
        return ESP_OK;
    }

    if (storage_prepare_workspace_layout() != ESP_OK) {
        ESP_LOGE(TAG, "failed to create workspace directories");
        ESP_LOGW(TAG, "shell will start from protected system path; use RENEW to rebuild workspace");
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
    err = esp_littlefs_format(STORAGE_WORKSPACE_PARTITION_LABEL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to format workspace partition: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "renew workspace: formatted %s", STORAGE_WORKSPACE_PARTITION_LABEL);

    ESP_LOGI(TAG, "renew workspace: remounting %s at %s", STORAGE_WORKSPACE_PARTITION_LABEL, STORAGE_WORKSPACE_MOUNT_POINT);
    err = storage_mount_workspace();
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

bool storage_workspace_usage_bytes(size_t *total_bytes, size_t *used_bytes, size_t *free_bytes)
{
    size_t total = 0;
    size_t used = 0;
    if (!s_workspace_ready ||
        esp_littlefs_info(STORAGE_WORKSPACE_PARTITION_LABEL, &total, &used) != ESP_OK ||
        used > total) {
        return false;
    }

    if (total_bytes) {
        *total_bytes = total;
    }
    if (used_bytes) {
        *used_bytes = used;
    }
    if (free_bytes) {
        *free_bytes = total - used;
    }
    return true;
}

bool storage_workspace_free_bytes(size_t *free_bytes)
{
    return free_bytes && storage_workspace_usage_bytes(NULL, NULL, free_bytes);
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
//Keep Going.
