# ESP32-C3 Micro Linux Shell Port Contract

The shell is intentionally small, but it is not a complete computer by itself.
To rebuild it as a standalone ESP32-C3 shell project, provide the APIs and
runtime services below.

## Required C API

The shell includes these headers by name:

```c
#include "input.h"
#include "storage.h"
```

Your project must provide compatible declarations and implementations.
Reference declarations are included in this package under `port/`; production
firmware can either use those headers directly or provide equivalent project
headers.

### input.h

Required functions:

```c
esp_err_t input_init(void);
int input_read_line(char *buf, size_t len);
void input_write(const char *text);
void input_write_bytes(const void *data, size_t len);
```

Behavior contract:

- `input_init()` prepares the active console.
- `input_read_line()` returns one line without the trailing newline.
- Overlong input must be drained to the next newline before returning.
- `input_write()` and `input_write_bytes()` must be safe from shell context.
- USB Serial/JTAG must remain available as the recovery console.

Optional future BLE boundary:

```c
esp_err_t input_ble_hid_init(void);
bool input_ble_hid_ready(void);
bool input_ble_hid_boot_key_to_ascii(uint8_t modifier, uint8_t keycode, char *ascii);
```

### storage.h

Required constants:

```c
#define STORAGE_SYSTEM_MOUNT_POINT     "/system"
#define STORAGE_WORKSPACE_MOUNT_POINT  "/workspace"
#define STORAGE_SYSTEM_PARTITION_LABEL "system_fs"
#define STORAGE_WORKSPACE_PARTITION_LABEL "workspace_fs"
#define STORAGE_PATH_MAX 160
```

Required functions:

```c
esp_err_t storage_init(void);
esp_err_t storage_renew_workspace(void);
bool storage_workspace_ready(void);
bool storage_resolve_path(const char *input, char *out, size_t out_size);
bool storage_normalize_workspace_path(const char *cwd, const char *input, char *out, size_t out_size);
bool storage_resolve_workspace_path(const char *cwd, const char *input, char *out, size_t out_size);
```

Behavior contract:

- Public shell file commands must resolve inside `/workspace`.
- `..` and absolute paths must not escape `/workspace`.
- `/` means the workspace root from the user's point of view.
- `storage_renew_workspace()` formats only `workspace_fs`.
- Boot must not silently format a damaged workspace.
- `storage_workspace_ready()` tells the shell whether to print the recovery
  warning after the banner.

## Standalone Port Included Here

This folder includes a complete standalone ESP32-C3 micro Linux shell port:

```text
main/input_serial.c
main/storage_fatfs.c
partitions.csv
sdkconfig.defaults
```

It uses USB Serial/JTAG for input and ESP-IDF FATFS + wear levelling for the
two flash data partitions. It is intended as the minimal rebuildable ESP32-C3
micro Linux shell project.

## Required ESP-IDF Features

- ESP-IDF 5.3.x or compatible ESP-IDF version.
- POSIX/VFS functions used by `shell.c`:
  - `opendir`
  - `readdir`
  - `closedir`
  - `stat`
  - `mkdir`
  - `rmdir`
  - `rename`
  - `unlink`
- LittleFS or another VFS mounted at `/workspace` with directory support.

## Required Partitions

The current C3 project uses:

```text
system_fs
workspace_fs
```

`RENEW` must format only `workspace_fs`; it must never erase application
firmware, `system_fs`, NVS, or OTA data.

## Entry Point

For the standalone port, call the services in this order from the application:

```c
storage_init();
shell_run();
```

`shell_run()` initializes the input backend and does not return during normal
operation.

## Optional External Commands

A parent firmware can register an external command handler before `shell_run()`:

```c
shell_register_external_exec(bin_exec_line);
```

The root `C3_Basic_Computer` firmware uses this hook for `/bin` services such
as `/bin/hardware`. This hook is optional. A standalone micro Linux shell port
should omit it unless that project intentionally provides external commands.

External commands must stay outside the shell core and must not be added to the
standalone shell `HELP` output unless they become real shell built-ins.
