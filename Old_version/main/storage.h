#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>

#define STORAGE_SYSTEM_MOUNT_POINT     "/system"
#define STORAGE_WORKSPACE_MOUNT_POINT  "/workspace"
#define STORAGE_SYSTEM_PARTITION_LABEL "system_fs"
#define STORAGE_WORKSPACE_PARTITION_LABEL "workspace_fs"

#define STORAGE_MOUNT_POINT STORAGE_WORKSPACE_MOUNT_POINT

esp_err_t storage_init(void);
esp_err_t storage_renew_workspace(void);
bool storage_resolve_path(const char *input, char *out, size_t out_size);
