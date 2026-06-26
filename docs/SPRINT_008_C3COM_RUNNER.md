# Sprint 008 Task List: Guarded C3 `.com` Runner

Status: Board-tested

Goal: run explicitly packaged C3 `.com` programs from the shell after YMODEM
transfer, without auto-running transferred files and without accepting raw
unvalidated binaries.

## Command Contract

```text
RUN /bin/name.com [args...]
```

Rules:

- `RUN` requires an explicit `.com` path.
- `RUN` accepts whitespace-separated arguments and passes them to the program.
- First slice supports up to 8 arguments including `argv[0]`.
- Files must use the C3COM header in `source/shell/c3_com.h`.
- Header validation checks magic, ABI version, ESP32-C3 RV32 target, code
  offset, code size, entry offset, and CRC32.
- Code is copied to executable RAM before jumping.
- ESP32-C3 native `.com` execution requires
  `CONFIG_ESP_SYSTEM_MEMPROT_FEATURE` disabled so `MALLOC_CAP_EXEC` heap is
  available.
- First-slice `.com` programs must be position-independent and avoid absolute
  `.rodata` references, or they will fault after relocation.
- Standard I/O is provided through ABI callbacks:
  - `stdin_read_line`
  - `stdout_write`
  - `stderr_write`
- `.com` transfer and `.com` execution remain separate steps.

## Non-Goals

- No auto-run after `RECV`.
- No raw binary execution.
- No filesystem write API inside `.com` yet.
- No hardware API inside `.com` yet.
- No process isolation or preemptive multitasking.
- No shell command access from `.com` yet.
- No relocation loader yet.

## Task List

### C8-T1 - Define C3COM ABI header

- [x] Add `source/shell/c3_com.h`.
- [x] Define `C3CM` magic, ABI version, target, code offset, code size, entry
  offset, CRC32, and reserved fields.
- [x] Define `c3_com_api_t` with argv and standard I/O callbacks.
- [x] Define entry shape:
  `int entry(const c3_com_api_t *api, int argc, const char *const argv[])`.

### C8-T2 - Add guarded shell RUN command

- [x] Add `SHELL_COMMAND_RUN`.
- [x] Add `RUN` to command lookup and `HELP`.
- [x] Require `RUN /bin/name.com [args...]` syntax.
- [x] Reject non-`.com` paths.
- [x] Reject missing path with usage text.
- [x] Parse whitespace-separated argv.
- [x] Reject too many arguments.

### C8-T3 - Validate and load image

- [x] Check file exists and is not a directory.
- [x] Check file size against header and max code size.
- [x] Validate header magic, version, target, offsets, entry alignment, and CRC.
- [x] Allocate executable RAM and copy code.
- [x] Clear instruction cache before jump.
- [x] Free executable memory after return.

### C8-T4 - Provide minimal standard I/O ABI

- [x] Provide `stdout_write`.
- [x] Provide `stderr_write`.
- [x] Provide `stdin_read_line`.
- [x] Pass `argc` and `argv` both in API struct and entry arguments.

### C8-T5 - Host fixture builder and runner smoke

- [x] Add `tools/c3com_fixture_smoke.py` to build a tiny C3COM fixture.
- [x] Fixture should print argv through stdout and return 0.
- [x] Add optional serial `--smoke` mode.
- [x] Upload fixture with YMODEM.
- [x] Run `RUN /bin/fixture.com one two`.
- [x] Verify stdout and `EXIT 0`.
- [x] Add a bad-CRC fixture generator.
- [x] Verify bad-CRC rejection on board.

### C8-T6 - Documentation sync

- [x] Update:
  - `README.md`
  - `SESSION_STATUS.md`
  - `source/shell/README.md`
  - `source/shell/COMMAND_API.md`
  - `source/shell/TEST_CASES.md`
  - `.codex/PROJECT_SKILL.md`
- [x] Document that `RUN` is guarded C3COM execution, not BASIC immediate mode.
- [x] Document that `.com` transfer does not imply execution.

### C8-T7 - Verification

- [x] Build firmware:
  ```bash
  tools/idf53.sh -B build-c3-root build
  ```
- [x] Compile smoke tools.
- [x] Board-test `DF`, YMODEM transfer, and C3COM `RUN` on `/dev/ttyACM0`.

Board result: YMODEM upload/download passed, valid C3COM fixture printed
`argc` and `argv`, returned `EXIT 0`, and a bad-CRC fixture was rejected as
`Invalid .com image`.

## Next Milestone

After YMODEM transfer and guarded C3COM execution are board-tested, resume ASM
nano mode in
[`docs/SPRINT_009_ASM_NANO_MODE_TASK_LIST.md`](docs/SPRINT_009_ASM_NANO_MODE_TASK_LIST.md).

Keep Going.
