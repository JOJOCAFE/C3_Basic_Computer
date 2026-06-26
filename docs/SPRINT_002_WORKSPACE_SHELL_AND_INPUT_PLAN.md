# Sprint 002 Planning: Workspace Shell and Input Boundary

Status: Complete, board-tested

Execution queue:

```text
docs/SPRINT_002_TASK_LIST.md
```

## Purpose

This sprint plan connects three design decisions:

1. The computer uses two internal storage partitions:
   - `system_fs` mounted at `/system`
   - `workspace_fs` mounted at `/workspace`
2. The Shell should gain the useful structure of a small micro Linux workspace shell.
3. Input must start with a PC terminal and later accept a BLE HID keyboard without changing shell logic.

This sprint is complete. The shell-first track now gates the next work: ASM
capture can resume as a non-execution milestone, while native execution and
monitor tooling remain later guarded milestones.

## Source Reference

OpenC6 BIOS, forked at:

```text
https://github.com/JOJOCAFE/openc6-bios
```

Useful source shape:

```text
components/boot_manager/boot_menu.c
docs/boot_manager.md
```

Use OpenC6 as a structure reference only. Do not copy its boot manager, XIP execution path, UART1 routing, or `openc6_fs` backend into C3.

Reference concepts to reuse:

1. Current-directory state.
2. Path resolver before command handlers.
3. One command loop that returns to prompt after each command.
4. Small command handlers for file operations.
5. Bounded reads for `cat`/copy-like commands.

Reference concepts to reject for this sprint:

1. Public `format` command.
2. `boot ram` / `boot xip`.
3. Payload ABI jump table.
4. XIP flash mapping.
5. UART1-only console redirection.
6. Raw OpenC6 file-system backend.

## Architecture Rule

The C3 shell must depend on C3 services, not OpenC6 services.

```text
Shell
  |
  +-- Workspace commands
  |
  +-- Storage services
  |     |
  |     +-- /workspace on workspace_fs
  |
  +-- Input service
        |
        +-- USB serial terminal first
        +-- BLE HID keyboard later
```

## Storage Boundary

The storage contract remains:

```text
/system      read-mostly system boundary
/workspace   maker-owned writable workspace
```

Rules:

1. Boot and protected recovery start from the system side, not from user workspace state.
2. Shell file commands operate under `/workspace`.
3. No shell command writes to `/system`.
4. User workspace may be damaged, deleted, or made unusable without breaking the protected shell path.
5. If `/workspace` mounts and passes layout bootstrap, the shell enters normal workspace mode.
6. If `/workspace` is damaged, the shell still starts and tells the user to run `RENEW`.
7. `RENEW` is protected system behavior; it must not depend on files stored in `/workspace`.
8. `RENEW` formats only `workspace_fs`.
9. `RENEW` remains the destructive recovery command, with two-step confirmation.
10. Do not add a normal `FORMAT` command to the public shell.

## Boot And Renew Invariant

The C3 must always preserve this route:

```text
Power on
  -> protected firmware/system path starts shell
  -> try mount workspace_fs
      -> if OK: normal workspace shell
      -> if bad: protected shell still starts
              -> user may run RENEW
              -> RENEW asks twice
              -> RENEW formats workspace_fs only
              -> workspace layout is recreated
```

The user can break the workspace. The user must not be able to delete, replace,
or strand the `RENEW` command by changing workspace files.

## Shell Direction

C3 should borrow the OpenC6 shell structure:

1. Maintain a current working directory.
2. Read one command line.
3. Parse command and arguments.
4. Resolve paths safely.
5. Dispatch to small command handlers.
6. Return to the READY prompt after each command.

C3 should not copy OpenC6 command behavior exactly. C3 remains a BASIC computer,
but the shell must become useful enough as a small file workspace before BASIC
or ASM work continues.

## Command Compatibility Map

| OpenC6 command | C3 shell command | Sprint 002 decision |
| --- | --- | --- |
| `help` | `HELP` | keep and update |
| `ls [path]` | `LS [path]` | boot shell exposes `LS`; `DIR` is not exposed |
| `cd <path>` | `CD <path>` | add |
| `mkdir <path>` | `MKDIR <path>` | add |
| `write [-f] <path> <text>` | `WRITE [-F] <path> <text>` | add after read/list commands |
| `cp <src> <dst>` | `CP <src> <dst>` | boot shell exposes `CP`; `COPY` is not exposed |
| `mv <src> <dst>` | `MV <src> <dst>` | boot shell exposes `MV`; `MOVE` is not exposed |
| `cat <path>` | `CAT <path>` | add bounded read |
| `rm <path>` | `RM <path>`, `RM -R <path>` | boot shell exposes `RM`; `DELETE` is not exposed |
| `format` | `RENEW` only | reject public `FORMAT` |
| `boot <ram\|xip>` | later ASM/runtime sprint | reject in Sprint 002 |
| `exit` | no public exit yet | optional later `RESET`/`REBOOT` design |

## First Workspace Shell Commands

Implement the first group before advanced monitor or native execution commands:

```text
PWD
LS [path]
CD <path>
MKDIR <path>
RMDIR <dir>
CAT <file>
```

Acceptance:

1. `PWD` prints the current workspace-relative directory.
2. `LS` lists the current workspace directory.
3. `LS` prints directories with a leading `/` and files without one.
4. `CD basic` changes to `/workspace/basic`.
5. `CD /asm` changes to `/workspace/asm`.
6. `CD ..` never escapes `/workspace`.
7. `MKDIR data/test` creates only under `/workspace`.
8. `RMDIR temp/empty` removes an empty directory only.
9. `CAT basic/hello.bas` prints file content in bounded chunks.
10. Bad paths return short text errors and keep the shell alive.

## Second Workspace Shell Commands

Add only after the first group is stable:

```text
WRITE [-F] <path> <text>
RM <path>
RM -R <path>
CP <src> <dst>
MV <src> <dst>
```

Acceptance:

1. `WRITE` creates text files only under `/workspace`.
2. Overwrite behavior requires `-F` or another explicit design choice.
3. `RM` deletes files and does not remove directories unless `-R` is used.
4. Copy and move operate only under `/workspace`.
5. Directory copy is rejected until explicitly designed.
6. Existing destination behavior is explicit.
7. Large files are copied in bounded chunks.

## Commands Not in This Sprint

Do not implement:

```text
BOOT
XIP
RAMBOOT
FORMAT
PXE
OTA
```

Reason:

1. Native execution belongs after ASM capture and validation.
2. Raw formatting conflicts with the recoverability model.
3. Network boot and OTA are not required for the PC-terminal shell milestone.

## Input Boundary

Shell input must be routed through an input service before BLE work starts.

Implemented backend:

```text
PC terminal
USB Serial/JTAG
line input
```

Hardware-pending backend:

```text
BLE HID keyboard
ESP-IDF esp_hid host
key mapper
input queue
```

The local ESP-IDF tree contains:

```text
/home/jo/esp-idf/examples/bluetooth/esp_hid_host
/home/jo/esp-idf/components/esp_hid
```

These are the reference starting points for the BLE HID keyboard driver.

## Input Service Tasks

### I1 - Define input event type

File target:

```text
main/input.h
```

Fields:

```text
key
modifier
pressed
ascii
```

Done when:

1. Shell does not need to know whether input came from USB serial or BLE.
2. Printable characters, Enter, Backspace, and arrows have stable names.

### I2 - USB serial input backend

File target:

```text
main/input_serial.c
```

Done when:

1. Current PC terminal behavior still works.
2. Existing line editing behavior is preserved.
3. Shell can consume input through the new input API.

### I3 - BLE HID keyboard backend

File target:

```text
main/input_ble_hid.c
```

Done when:

1. BLE keyboard pairing is isolated from shell logic.
2. HID reports map into C3 key events.
3. Disconnect/reconnect does not crash the shell.
4. PC terminal remains available for development and recovery.

## Completed Implementation Order

1. Keep partition and `RENEW` behavior locked.
2. Keep adversarial shell flood/path tests green.
3. Add workspace path state and resolver.
4. Add `PWD`.
5. Add `LS` directory listing.
6. Add `CD`.
7. Add `MKDIR`.
8. Add `CAT`.
9. Add `WRITE`.
10. Add `RM`, `RM -R`, and `RMDIR`.
11. Add `CP`.
12. Add `MV`.
13. Refactor shell input behind an input service.
14. Add BLE HID backend boundary only after USB serial input still passes smoke tests.

Completion note:

1. Items 1-13 are implemented and board-tested over USB Serial/JTAG.
2. Item 14 is reference-reviewed and compiled as an inactive boundary.
3. Real BLE keyboard pairing is pending until a keyboard is available.

## Review Checkpoint

Current decisions:

1. This sprint ran before Phase 2A ASM capture.
2. Keep current C3 command matching case-insensitive.
3. Document uppercase names first because current firmware presents uppercase commands.
4. Keep the boot shell as the implemented micro Linux command set only.
5. Legacy aliases such as `DIR`, `COPY`, `MOVE`, and `DELETE` are not exposed.

Sprint 002 is complete. Resume with guarded ASM capture or BLE keyboard pairing
when the required keyboard is available.
