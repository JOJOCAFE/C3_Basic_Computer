# Sprint 007 Task List: Shell YMODEM File Transfer

Status: Board-tested

Goal: add binary-safe file transfer over the existing terminal link so `.bas`,
`.asm`, `.com`, config, and test files can move to and from `/workspace`
without reflashing the ESP32-C3.

YMODEM is a protected shell feature, not a removable `/bin` service. File
transfer is infrastructure for repairing and testing the workspace, so it must
remain available even if optional services are disabled or broken.

## Command Contract

```text
DF
RECV [-F] <path>
SEND <path>
```

Examples:

```text
DF
RECV /basic/demo.bas
RECV -F /bin/test.com
SEND /basic/demo.bas
SEND /bin/test.com
```

Rules:

- Paths resolve through the shell workspace resolver and cannot escape
  `/workspace`.
- `DF` reports workspace capacity in Unix-style 1K-block output before a user
  starts transfer work.
- `RECV` receives one YMODEM file and saves it to the exact path supplied on
  the shell command line.
- The incoming YMODEM filename is ignored for path selection.
- `RECV` rejects an existing file unless `-F` is present.
- `RECV` checks workspace free space after the YMODEM metadata block and before
  accepting file data.
- `SEND` transmits the selected workspace file with basename and exact size.
- Transfer mode is binary-safe and may carry NUL bytes.
- No batch transfer in the first slice.
- No `.com` execution in this sprint.

## Task List

### Y7-T1 - Freeze transfer command contract

- [x] Choose YMODEM over FTP for terminal-first file movement.
- [x] Keep transfer in the shell, not `/bin`.
- [x] Define `RECV [-F] <path>` and `SEND <path>`.
- [x] Keep all paths constrained to `/workspace`.
- [x] Put ASM nano mode after shell transfer.

### Y7-T2 - Add raw input port API

- [x] File: `main/input.h`
- [x] File: `main/input_serial.c`
- [x] File: `source/shell/port/input.h`
- [x] File: `source/shell/main/input_serial.c`
- [x] Add `input_read_bytes()` for raw serial transfer reads.
- [x] Preserve current `input_read_line()` behavior.

### Y7-T3 - Add shell command enum and help

- [x] File: `source/shell/shell.h`
- [x] File: `source/shell/shell.c`
- [x] Add `SHELL_COMMAND_DF`.
- [x] Add `SHELL_COMMAND_RECV`.
- [x] Add `SHELL_COMMAND_SEND`.
- [x] Add `DF` lookup.
- [x] Add `RECV` and `SEND` lookup.
- [x] Add `DF`, `RECV`, and `SEND` to `HELP`.

### Y7-T3A - Add free-space visibility

- [x] File: `main/storage.h`
- [x] File: `main/storage.c`
- [x] File: `source/shell/port/storage.h`
- [x] File: `source/shell/main/storage_fatfs.c`
- [x] Add storage usage API for total, used, and free workspace bytes.
- [x] Add shell `DF` output:
  `Filesystem 1K-blocks Used Available Mounted on`
- [x] Use the same storage usage backend for `DF` and `RECV` preflight.

### Y7-T4 - Implement single-file YMODEM receive

- [x] File: `source/shell/shell.c`
- [x] Check free workspace space before accepting file data.
- [x] Receive YMODEM CRC blocks.
- [x] Parse filename/size metadata block.
- [x] Ignore incoming filename for destination path selection.
- [x] Write exactly the declared file size.
- [x] Reject missing size metadata.
- [x] Reject existing destination unless `-F` is used.
- [x] Remove partial destination on failed transfer.

### Y7-T5 - Implement single-file YMODEM send

- [x] File: `source/shell/shell.c`
- [x] Send YMODEM metadata block with basename and exact file size.
- [x] Send file data in 1 KiB blocks with padding only on the wire.
- [x] Send terminating empty metadata block.
- [x] Reject directories and oversized files.

### Y7-T6 - Add host transfer smoke

- [x] Add `tools/ymodem_transfer_smoke.py`.
- [x] Upload a text `.bas` file to `/basic`.
- [x] Upload a binary `.com` fixture to `/bin`.
- [x] Download both files.
- [x] Verify byte-for-byte equality on the host.
- [x] Confirm `RECV` without `-F` rejects an existing file.

### Y7-T7 - Documentation sync

- [x] Update:
  - `README.md`
  - `SESSION_STATUS.md`
  - `source/shell/README.md`
  - `source/shell/COMMAND_API.md`
  - `source/shell/TEST_CASES.md`
  - `.codex/PROJECT_SKILL.md`
- [x] Record that transfer is shell infrastructure.
- [x] Record that `.com` movement is supported separately from `.com`
  execution.

### Y7-T8 - Verification

- [x] Compile Python smoke tools:
  ```bash
  python3 -m py_compile tools/ymodem_transfer_smoke.py
  ```
- [x] Build firmware:
  ```bash
  tools/idf53.sh -B build-c3-root build
  ```
- [x] Board smoke on `/dev/ttyACM0`:
  ```bash
  tools/idf53.sh -B build-c3-root -p /dev/ttyACM0 flash
  python3 tools/ymodem_transfer_smoke.py --port /dev/ttyACM0
  ```

Board result: `DF` printed workspace capacity, `RECV` uploaded `.bas` and
`.com` files, overwrite without `-F` was rejected, `SEND` downloaded both files
byte-for-byte, and cleanup removed the smoke files.

## Next Milestone

After shell YMODEM transfer is built and board-tested, continue with guarded
C3COM execution in
[`docs/SPRINT_008_C3COM_RUNNER.md`](docs/SPRINT_008_C3COM_RUNNER.md), then
resume ASM nano mode in
[`docs/SPRINT_009_ASM_NANO_MODE_TASK_LIST.md`](docs/SPRINT_009_ASM_NANO_MODE_TASK_LIST.md).

Keep Going.
