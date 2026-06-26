# Sprint 002 Task List: Partition, Workspace Shell, Keyboard Input

Status: Complete

Use this as the completion record for the shell-first partition + keyboard
track. BASIC and ASM expansion were deferred until this Micro UNIX-style shell
surface became complete and board-tested.

## S2-T0 - Close partition renew observability

Status: DONE (code-reviewed, revised for protected renew path)

- Files:
  - `Old_version/main/storage.c`
  - `docs/SPRINT1_TICKET_LIST.md`
- Action:
  1. Add positive logs for workspace renew sub-steps.
  2. Mark partition observability closed after code review.
- Done when:
  - Renew logs unmount, format, remount, and workspace layout recreation.
  - R1-T9 is no longer partial.
  - Boot does not silently format a damaged workspace.
  - A damaged workspace still leaves the protected shell and `RENEW` path available.

## S2-T0A - Protected renew boot path

Status: DONE (code-reviewed)

- Files:
  - `Old_version/main/storage.c`
  - `Old_version/main/storage.h`
  - `Old_version/main/shell.c`
  - `Old_version/tools/renew_full_smoke.py`
- Action:
  1. Keep `system_fs` mounted read-only from shell context.
  2. Disable automatic workspace formatting during boot.
  3. Let shell start even when workspace mount/layout fails.
  4. Keep `RENEW` as the two-confirmation repair path for `workspace_fs`.
- Done when:
  - User files in `/workspace` cannot delete or replace the `RENEW` command.
  - Boot tries workspace, but does not silently repair it.
  - If workspace is bad, shell prints `WORKSPACE ERROR. Run RENEW to rebuild workspace.`
  - `RENEW` formats only `workspace_fs` after both confirmations.
- Board verification:
  - 2026-06-26: flashed `build-c3-basic` to `/dev/ttyACM0`.
  - 2026-06-26: healthy-workspace `dir_delete_smoke.py` passed.
  - 2026-06-26: healthy-workspace `load_list_run_smoke.py` passed.
  - 2026-06-26: healthy-workspace `reboot_persistence_smoke.py` passed.
  - 2026-06-26: healthy-workspace `adversarial_shell_smoke.py` passed.
  - 2026-06-26: destructive full `RENEW` test passed with `renew_full_smoke.py`.
  - 2026-06-26: post-RENEW `dir_delete_smoke.py` passed.
  - 2026-06-26: post-RENEW `load_list_run_smoke.py` passed.
  - 2026-06-26: post-RENEW `reboot_persistence_smoke.py` passed.
  - Damaged-workspace boot simulation is still a future destructive/corruption test.

## S2-T1 - Workspace shell path state design

Status: DONE (code-reviewed)

- Files:
  - `Old_version/main/storage.h`
  - `Old_version/main/storage.c`
  - `Old_version/main/shell.c`
- Action:
  1. Add shell current-directory state under `/workspace`.
  2. Add path resolver for workspace shell commands.
  3. Keep resolver from escaping `/workspace`.
- Done when:
  - `/`, `.`, and `..` resolve safely.
  - `CD ..` from workspace root stays at workspace root.
  - Existing `LOAD`, `SAVE`, `DELETE`, `DIR`, and `RENEW` behavior remains unchanged.
- Evidence:
  - `storage_normalize_workspace_path()` resolves workspace-relative paths without escaping `/workspace`.
  - `storage_resolve_workspace_path()` maps normalized workspace paths under `/workspace`.
  - Shell current-directory state initializes to `/`.
  - Existing command dispatch still uses the previous `LOAD`, `SAVE`, `DELETE`, `DIR`, and `RENEW` paths.
- Board verification:
  - 2026-06-26: flashed `build-c3-basic` to `/dev/ttyACM0`.
  - 2026-06-26: `dir_delete_smoke.py` passed.
  - 2026-06-26: `load_list_run_smoke.py` passed.
  - 2026-06-26: `reboot_persistence_smoke.py` passed.
  - 2026-06-26: `adversarial_shell_smoke.py` passed after fixes for malformed numeric input and overlong serial lines.
  - `RENEW` was not run in the smoke pass because it intentionally deletes workspace data.

## S2-T1A - Adversarial shell hardening

Status: DONE (board-tested)

- Files:
  - `Old_version/main/basic.c`
  - `Old_version/main/shell.c`
  - `Old_version/tools/adversarial_shell_smoke.py`
- Action:
  1. Inject nonsense commands, unsafe paths, RENEW abort input, overlong serial lines, binary-ish input, and fast command bursts.
  2. Fix any shell/parser behavior that accepts malformed input or loses prompt synchronization.
- Bugs found:
  - `123ABC` was accepted as a BASIC line because numeric line parsing did not require whitespace or end-of-line after the line number.
  - Overlong serial input was split into multiple commands because the line reader returned when its buffer filled instead of draining to CR/LF.
- Done when:
  - Malformed numeric input returns `LINE ERROR`.
  - Overlong input returns one clean shell error and the next prompt remains synchronized.
  - Existing storage and BASIC smoke tests still pass on the board.
- Board verification:
  - 2026-06-26: flashed `build-c3-basic` to `/dev/ttyACM0`.
  - 2026-06-26: `adversarial_shell_smoke.py` passed.
  - 2026-06-26: `dir_delete_smoke.py` passed.
  - 2026-06-26: `load_list_run_smoke.py` passed.
  - 2026-06-26: `reboot_persistence_smoke.py` passed.

## S2-T2 - Add PWD

Status: DONE (board-tested)

- File: `Old_version/main/shell.c`
- Action:
  1. Add `PWD` command.
  2. Print current workspace-relative path.
- Done when:
  - At boot, `PWD` prints `/`.
  - The command returns to the shell prompt.
- Evidence:
  - `shell_help()` lists `PWD`.
  - `shell_pwd()` prints the current workspace-relative directory.
  - `shell_execute_line()` dispatches `PWD` case-insensitively.
- Board verification:
  - 2026-06-26: `workspace_shell_smoke.py` passed.

## S2-T3 - Add LS alias and current-directory listing

Status: DONE (board-tested)

- File: `Old_version/main/shell.c`
- Action:
  1. Add `LS [path]` as the Micro UNIX-style alias.
  2. Preserve existing `DIR [path]`.
  3. Let no-argument `LS` list the current workspace directory.
- Done when:
  - `LS` at boot lists `/workspace`.
  - `LS basic` lists `/workspace/basic`.
  - `DIR` behavior remains compatible with existing smoke tests.
  - Bad paths print a short error and keep shell responsive.
- Board verification:
  - 2026-06-26: `workspace_shell_smoke.py` passed.
  - 2026-06-26: `dir_delete_smoke.py` passed.

## S2-T4 - Add CD

Status: DONE (board-tested)

- File: `Old_version/main/shell.c`
- Action:
  1. Add `CD <path>` command.
  2. Support absolute workspace paths and relative paths.
- Done when:
  - `CD basic` enters `/basic`.
  - `CD /asm` enters `/asm`.
  - `CD ..` cannot leave workspace.
  - Bad paths print a short error and keep shell responsive.
- Board verification:
  - 2026-06-26: `workspace_shell_smoke.py` passed.

## S2-T5 - Add MKDIR

Status: DONE (board-tested)

- File: `Old_version/main/shell.c`
- Action:
  1. Add `MKDIR <path>` command.
  2. Create directories only under `/workspace`.
- Done when:
  - `MKDIR data/test` creates `/workspace/data/test`.
  - Existing path conflicts return a short error.
- Board verification:
  - 2026-06-26: `workspace_shell_smoke.py` passed.

## S2-T6 - Add CAT

Status: DONE (board-tested)

- File: `Old_version/main/shell.c`
- Action:
  1. Add `CAT <file>` command.
  2. Print file content in bounded chunks.
- Done when:
  - `CAT basic/hello.bas` prints file content.
  - Large files do not require one large allocation.
  - Directories are rejected.
- Board verification:
  - 2026-06-26: `workspace_shell_smoke.py` passed.

## S2-T7 - Add WRITE

Status: DONE (board-tested)

- File: `Old_version/main/shell.c`
- Reference:
  - OpenC6 `write [-f] <path> <txt>`
- Action:
  1. Add `WRITE <path> <text>`.
  2. Add explicit overwrite behavior with `-F` if overwrite is supported.
  3. Keep writes constrained to `/workspace`.
- Done when:
  - `WRITE temp/hello.txt hello` creates a text file.
  - Existing destination behavior is explicit.
  - Writing to a directory is rejected.
  - Unsafe paths are rejected.
- Board verification:
  - 2026-06-26: `workspace_shell_smoke.py` passed.

## S2-T8 - Add RM alias and delete policy

Status: DONE (board-tested)

- File: `Old_version/main/shell.c`
- Reference:
  - OpenC6 `rm <path>`
- Action:
  1. Add `RM <path>` as the Micro UNIX-style alias.
  2. Preserve existing `DELETE <path>`.
  3. Keep directory deletion policy explicit.
- Done when:
  - `RM temp/hello.txt` deletes a file.
  - `DELETE temp/hello.txt` still works.
  - Non-empty directory deletion is rejected unless separately designed.
  - Unsafe paths are rejected.
- Board verification:
  - 2026-06-26: `workspace_shell_smoke.py` passed.
  - 2026-06-26: `dir_delete_smoke.py` passed.

## S2-T9 - Add COPY / CP

Status: DONE (board-tested)

- File: `Old_version/main/shell.c`
- Reference:
  - OpenC6 `cp <src> <dst_path>`
- Action:
  1. Add `COPY <src> <dst>`.
  2. Add `CP <src> <dst>` alias.
  3. Copy files in bounded chunks.
- Done when:
  - Copy operates only under `/workspace`.
  - Directory copy is rejected.
  - Existing destination behavior is explicit.
  - Large files do not require one large allocation.
- Board verification:
  - 2026-06-26: `workspace_shell_smoke.py` passed.

## S2-T10 - Add MOVE / MV

Status: DONE (board-tested)

- File: `Old_version/main/shell.c`
- Reference:
  - OpenC6 `mv <src> <dst_path>`
- Action:
  1. Add `MOVE <src> <dst>`.
  2. Add `MV <src> <dst>` alias.
  3. Move or rename files without crossing the workspace boundary.
- Done when:
  - Move operates only under `/workspace`.
  - Directory move is rejected until separately designed.
  - Existing destination behavior is explicit.
  - Source is gone and destination exists after successful move.
- Board verification:
  - 2026-06-26: `workspace_shell_smoke.py` passed.
  - 2026-06-26: `load_list_run_smoke.py` passed.
  - 2026-06-26: `reboot_persistence_smoke.py` passed.
  - 2026-06-26: `adversarial_shell_smoke.py` passed after HELP expectation update.

## S2-T11 - Update HELP and command reference

Status: DONE (board-tested)

- Files:
  - `Old_version/main/shell.c`
  - `docs/04_Shell_Reference.md`
  - `Old_version/README.md`
- Action:
  1. Make `HELP` show the implemented shell command set.
  2. Mark BASIC and ASM expansion as later work.
  3. Keep command naming clear: C3 uppercase first, Unix aliases where implemented.
- Done when:
  - `HELP` output matches implemented commands.
  - Docs do not claim unimplemented ASM/native execution as current.
- Evidence:
  - `Old_version/main/shell.c` HELP output lists the implemented S2 shell command set.
  - `docs/04_Shell_Reference.md` lists C3 uppercase commands and Unix aliases.
  - `Old_version/README.md` lists the implemented shell commands.
- Board verification:
  - 2026-06-26: `dir_delete_smoke.py` observed updated HELP output.
  - 2026-06-26: `adversarial_shell_smoke.py` passed with updated HELP output.

## S2-T12 - Add shell command smoke test

Status: DONE (board-tested)

- File: `Old_version/tools/workspace_shell_smoke.py`
- Action:
  1. Exercise `PWD`, `LS`, `CD`, `MKDIR`, `WRITE`, `CAT`, `RM`, `COPY`, and `MOVE`.
  2. Include unsafe-path rejects for every file command.
  3. Verify prompt recovery after each command.
- Done when:
  - The shell command smoke passes on `/dev/ttyACM0`.
  - Existing BASIC/storage/persistence/adversarial smokes still pass.
- Board verification:
  - 2026-06-26: `workspace_shell_smoke.py` passed.
  - 2026-06-26: `dir_delete_smoke.py` passed.
  - 2026-06-26: `load_list_run_smoke.py` passed.
  - 2026-06-26: `reboot_persistence_smoke.py` passed.
  - 2026-06-26: `adversarial_shell_smoke.py` passed.

## S2-T13 - Add input service boundary

Status: DONE (board-tested)

- Files:
  - `Old_version/main/input.h`
  - `Old_version/main/input_serial.c`
  - `Old_version/main/input_ble_hid.c`
  - `Old_version/main/shell.c`
  - `Old_version/main/CMakeLists.txt`
- Action:
  1. Move current USB Serial/JTAG line input behind an input API.
  2. Preserve existing terminal behavior.
- Done when:
  - Shell reads through the input API.
  - PC terminal remains the active backend.
  - Existing smoke tests still pass.
- Evidence:
  - `shell.c` uses `input_init()`, `input_read_line()`, `input_write()`, and `input_write_bytes()`.
  - `input_serial.c` owns the USB Serial/JTAG line editor and output path.
  - `input.h` defines `input_event_t` for future keyboard backends.
  - `input_ble_hid.c` builds as the future BLE HID backend boundary.
- Board verification:
  - 2026-06-26: build passed after input refactor.
  - 2026-06-26: flashed `build-c3-basic` to `/dev/ttyACM0`.
  - 2026-06-26: `workspace_shell_smoke.py` passed through the input API.
  - 2026-06-26: `dir_delete_smoke.py` passed through the input API.
  - 2026-06-26: `load_list_run_smoke.py` passed through the input API.
  - 2026-06-26: `reboot_persistence_smoke.py` passed through the input API before overflow hardening.
  - 2026-06-26: `adversarial_shell_smoke.py` exposed an overflow-drain regression after the input refactor.
  - 2026-06-26: `input_serial.c` overflow drain was hardened and `adversarial_shell_smoke.py` passed.
  - 2026-06-26: final `workspace_shell_smoke.py` and `load_list_run_smoke.py` passed after overflow hardening.

## S2-T14 - BLE HID keyboard reference review

Status: DONE (reference-reviewed, hardware pending)

- Local reference:
  - `/home/jo/Codex/ESP32-C3-BLE-Media-Macro-Pad`
  - `/home/jo/esp-idf/examples/bluetooth/esp_hid_host`
- Action:
  1. Review the macro-pad code and ESP-IDF HID host example.
  2. Decide whether C3 needs BLE HID host, BLE keyboard device, or both.
  3. Write the driver boundary before implementation.
- Done when:
  - BLE keyboard driver scope is documented.
  - Shell still does not depend directly on BLE APIs.
- Evidence:
  - `docs/SPRINT_002_BLE_HID_KEYBOARD_BOUNDARY.md` documents BLE HID host direction.
  - Macro-pad repo is treated as BLE HID device reference, not the C3 host driver.
  - ESP-IDF `esp_hid_host` is the host reference for receiving real keyboards.
  - `input_ble_hid.c` includes a boot-keyboard ASCII mapper and backend stubs.
  - Real BLE pairing is deferred until a keyboard is available.
  - Firmware builds with the BLE HID boundary compiled but inactive.

## Stop Rule

Do not implement native payload boot, XIP execution, PXE, OTA, or a public `FORMAT` command in this sprint.
