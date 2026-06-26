# ESP32-C3 Micro Linux Shell

This folder is a standalone shell package for the ESP32-C3. The shell is a
small micro Linux-style workspace shell: it gives the board a prompt, file
commands, safe workspace recovery, and a command API that other runtimes such as
BASIC can call through controlled APIs.

The shell is intentionally separate from BASIC and ASM. It does not expose BASIC
commands such as `PRINT`, `LIST`, or `RUN`.

## What This Folder Contains

```text
CMakeLists.txt                  Standalone ESP-IDF project file
c3_shell_sources.cmake          Reusable source/include list
shell.c                         Shell parser, command dispatch, file commands
shell.h                         Public shell API
idf53.sh                        ESP-IDF 5.3 launcher
partitions.csv                  Standalone flash partition table
sdkconfig.defaults              Standalone defaults
main/                           Standalone ESP32-C3 port
port/                           Reference input/storage API headers
standalone_tools/smoke.py       Board smoke test
COMMAND_API.md                  API for BASIC/other firmware callers
PORT_CONTRACT.md                Porting contract for another firmware project
REBUILD_CHECKLIST.md            Short rebuild checklist
TEST_CASES.md                   Expected behavior and regression cases
```

## Requirements

Hardware:

```text
ESP32-C3
USB Serial/JTAG console
4 MB flash
```

Validated software:

```text
ESP-IDF 5.3.x
ESP-IDF path: /home/jo/esp-idf
Serial port used during testing: /dev/ttyACM0
```

The standalone project in this folder uses:

```text
USB Serial/JTAG for the console
ESP-IDF FATFS + wear levelling for flash storage
/system mounted from system_fs
/workspace mounted from workspace_fs
```

The parent `C3_Basic_Computer` firmware uses the same shell source with its own
LittleFS storage backend.

The parent firmware may register external `/bin` services above the shell. The
current root firmware registers `/bin/hardware` through `source/bin` and
`source/hardware`. BASIC hardware statements call `source/hardware` directly
through `main/basic_hardware.*`; they are not part of the shell. The standalone
shell project does not link those folders.

## Build Standalone

Run these commands from this directory:

```bash
cd /home/jo/Codex/C3_Basic_Computer/source/shell
./idf53.sh -B build-standalone build
```

Flash the ESP32-C3:

```bash
./idf53.sh -B build-standalone -p /dev/ttyACM0 flash
```

Run the standalone board smoke test:

```bash
python3 standalone_tools/smoke.py --port /dev/ttyACM0
```

If your board appears on another port, replace `/dev/ttyACM0`.

## Build Inside C3_Basic_Computer

From the repository root:

```bash
cd /home/jo/Codex/C3_Basic_Computer
tools/idf53.sh -B build-c3-root build
tools/idf53.sh -B build-c3-root -p /dev/ttyACM0 flash
```

Run the parent firmware smoke tests after flashing:

```bash
python3 tools/workspace_shell_smoke.py --port /dev/ttyACM0
python3 tools/no_basic_shell_smoke.py --port /dev/ttyACM0
python3 tools/renew_full_smoke.py --port /dev/ttyACM0
python3 tools/adversarial_shell_smoke.py --port /dev/ttyACM0
python3 tools/bin_hardware_gpio_smoke.py --port /dev/ttyACM0 --pin 8 --seconds 10
```

## Boot Behavior

After boot, the shell prints:

```text
C3 BASIC COMPUTER
Version 0.1

READY.
>
```

The prompt is always:

```text
>
```

The current directory is not printed in the prompt. Use `PWD`.

## Workspace Model

The user sees `/` as the workspace root. Internally it maps to `/workspace`.

Default workspace layout:

```text
/
/basic
/asm
/bin
/config
/data
/temp
```

Rules:

- File commands stay inside `/workspace`.
- `..` and absolute paths must not escape the workspace.
- `LS` prints directories as `/name` and files as `name`.
- `RENEW` formats only `workspace_fs` after two confirmations.
- The shell must still start if the workspace is damaged, so the user can run
  `RENEW`.

## Commands

| Command | Use |
| --- | --- |
| `HELP` | Print implemented commands. |
| `PWD` | Print the current workspace-relative directory. |
| `LS [path]` | List a directory. |
| `CD <path>` | Change directory. |
| `MKDIR <path>` | Create a directory. |
| `RMDIR <dir>` | Remove an empty directory. |
| `CAT <file>` | Print a text file. |
| `WRITE <file> <text>` | Create a text file. |
| `WRITE -F <file> <text>` | Overwrite a text file. |
| `RM <file>` | Remove a file. |
| `RM -R <path>` | Recursively remove a workspace subtree. |
| `CP <src> <dst>` | Copy a file. |
| `MV <src> <dst>` | Move or rename a file or directory. |
| `RENEW` | Rebuild the workspace filesystem after confirmation. |

Examples:

```text
PWD
LS
LS basic
CD temp
WRITE note.txt hello
CAT note.txt
CP note.txt copy.txt
MV copy.txt moved.txt
RM moved.txt
CD /
MKDIR temp/test
RMDIR temp/test
```

Not exposed by this shell:

```text
DIR COPY MOVE DELETE RENAME
LOAD SAVE NEW LIST RUN PRINT
```

Parent firmware `/bin` services:

```text
/bin/hardware ...
```

These are registered above the shell by the root firmware. They are not part of
the standalone shell command set and must not appear in `HELP`.

## Command API

Other firmware code can call shell commands without entering the interactive
prompt loop:

```c
shell_exec_line("LS /basic", &io);
shell_exec_command(SHELL_COMMAND_PWD, NULL, &io);
```

Callers can capture output through `shell_exec_io_t.write`. The current BASIC
runtime uses a small whitelist for `SHELL "PWD"` and `SHELL "CAT <file>"`.
Broader `SYSTEM`-style access remains intentionally unavailable. See
[`COMMAND_API.md`](COMMAND_API.md).

## Porting To Another ESP32-C3 Project

To reuse the shell in another project:

1. Copy this `source/shell` folder.
2. Provide compatible `input.h` and `storage.h` APIs.
3. Mount a writable VFS at `/workspace`.
4. Provide `system_fs` and `workspace_fs` partitions.
5. Include `c3_shell_sources.cmake` from your component `CMakeLists.txt`.
6. Call `storage_init()` before `shell_run()`.

The detailed port requirements are in [`PORT_CONTRACT.md`](PORT_CONTRACT.md).

## Safety Checklist

Before accepting shell behavior changes:

```bash
./idf53.sh -B build-standalone build
python3 standalone_tools/smoke.py --port /dev/ttyACM0
```

For the parent firmware:

```bash
tools/idf53.sh -B build-c3-root build
python3 tools/workspace_shell_smoke.py --port /dev/ttyACM0
python3 tools/no_basic_shell_smoke.py --port /dev/ttyACM0
python3 tools/renew_full_smoke.py --port /dev/ttyACM0
python3 tools/adversarial_shell_smoke.py --port /dev/ttyACM0
python3 tools/bin_hardware_gpio_smoke.py --port /dev/ttyACM0 --pin 8 --seconds 10
```

Host-only syntax checks:

```bash
python3 -m py_compile standalone_tools/smoke.py
```
