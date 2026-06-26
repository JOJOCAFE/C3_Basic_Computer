# C3 BASIC COMPUTER Implementation Checklist (Strict)

This checklist is tied to the active root ESP-IDF project.
Use it as the execution plan so milestones remain auditable.

## 0. Baseline State (Do not change before passing)

- [x] `main/main.c` boots and calls `shell_run()`.
- [x] `main/storage.c` mounts split LittleFS partitions (`system_fs`, `workspace_fs`) and creates canonical workspace directories.
- [x] `source/shell/shell.c` offers micro Linux workspace commands: `HELP`, `PWD`, `LS`, `CD`, `MKDIR`, `RMDIR`, `CAT`, `WRITE`, `RM`, `RM -R`, `CP`, `MV`, plus protected `RENEW`.
- [x] `main/basic.c` supports tiny numbered BASIC, `:run`, `:debug`, and typed
  GPIO/ADC hardware functions/statements through `main/basic_hardware.*`.
- [x] `source/hardware` provides reusable GPIO, ADC, I2C, and SPI C service APIs.
- [x] `source/bin` exposes `/bin/hardware` terminal adapters for GPIO, ADC, I2C, and SPI.
- [x] `source/bin` has a `/bin` service registry with `/bin list`, `/bin/hardware`, and `/bin/nano`.
- [x] `source/editor` provides the first line-oriented `.txt` editor core for `EDIT`.
- [x] `README.md`, `source/shell/README.md`, and `source/shell/TEST_CASES.md` match current shell scope.

## 1. Move Implementation Surface to a Stable Workspace

Goal: keep new work out of legacy build artifacts and in versioned sources.

| File/Folder | Task | Acceptance Criteria |
| --- | --- | --- |
| `main/main.c` | Confirm boot path and storage init are unchanged before refactors. | Cold boot still shows READY prompt and shell banner on `/dev/ttyACM0` or PC terminal. |
| `main/CMakeLists.txt` | Keep source list in sync with new modules. | Build is link-complete after adding/removing files. |
| `tools/` | Keep required board tooling in sync with the current reference implementation. | `idf53.sh`, `workspace_shell_smoke.py`, `no_basic_shell_smoke.py`, `renew_full_smoke.py`, and `adversarial_shell_smoke.py` exist and run with port argument. |
| `docs/` | Mark progress status in implementation docs for files you will edit. | Each touched `.md` has matching behavior in code. |

## 2A. Completed Milestone - Hardware Service Boundary

Goal: provide reusable board hardware APIs without growing the shell core.

Rules:

- `source/hardware` owns GPIO, ADC, I2C, and SPI C APIs.
- `source/bin` owns terminal-facing command adapters such as `/bin/hardware`.
- `source/shell` remains standalone and does not link hardware/bin code.
- BASIC GPIO/ADC is implemented through `main/basic_hardware.*` and calls
  `source/hardware` directly.
- GPIO18/GPIO19 remain protected by default because they carry USB Serial/JTAG.

| File/Folder | Task | Acceptance Criteria |
| --- | --- | --- |
| `source/hardware/` | Keep GPIO/ADC/I2C/SPI service APIs isolated from shell. | Root firmware builds; standalone shell builds without hardware/bin. |
| `source/bin/` | Expose generic hardware terminal commands. | `/bin/hardware gpio`, `adc`, `i2c`, and `spi` commands build and return `OK` on valid calls. |
| `tools/bin_hardware_gpio_smoke.py` | Exercise GPIO through the board shell. | ESP32-C3 smoke returns `PASS: /bin hardware gpio`. |
| `source/hardware/COMMANDS.md` | Document current hardware terminal command syntax. | Examples match firmware output. |
| `source/hardware/VERIFICATION.md` | Record build, flash, and board-tested evidence. | Contains commands and observed results for current board. |

## 2B. Completed Milestone - Micro Linux Workspace Shell

Goal: provide the workspace shell first, using the OpenC6 shell as a
structure reference while keeping C3 storage, BASIC, and recovery rules intact.

OpenC6 reference shape:

- one line-input loop
- current-directory state
- path resolver
- small command handlers
- command dispatch table/chain

C3 adaptation rules:

- file commands operate only under `/workspace`
- `system_fs` remains read-mostly from the public shell
- boot and protected recovery must not depend on workspace files
- a damaged `workspace_fs` must not prevent the protected shell path from starting
- `RENEW` remains protected system behavior and the only public destructive workspace reset
- `RENEW` formats only `workspace_fs` after two confirmations
- boot must not silently auto-format `workspace_fs`; recovery goes through `RENEW`
- `FORMAT`, `BOOT`, `RAMBOOT`, `XIP`, `PXE`, OTA, and native payload execution stay out
- existing `HELP`, `PWD`, `LS`, `CD`, `MKDIR`, `RMDIR`, `CAT`, `WRITE`, `RM`,
  `RM -R`, `CP`, `MV`, and `RENEW` behavior must keep passing smoke tests
- BASIC immediate statements must not be exposed by the boot shell; BASIC runs
  through nano BASIC mode with `:run` and `:debug`

### 2.1 Shell command baseline

| File/Folder | Task | Acceptance Criteria |
| --- | --- | --- |
| `source/shell/shell.c` | Add `PWD`. | Prints `/` at boot and the current workspace-relative path after `CD`. |
| `source/shell/shell.c` | Add `CD <path>`. | Supports absolute and relative workspace paths; `CD ..` from root stays at `/`. |
| `source/shell/shell.c` | Add `LS [path]`. | Lists current or selected workspace directory; rejects paths outside `/workspace`; directories are prefixed with `/`. |
| `source/shell/shell.c` | Add `MKDIR <path>` and `RMDIR <dir>`. | Creates directories only inside `/workspace`; removes empty directories only; reports conflicts clearly. |
| `source/shell/shell.c` | Add `CAT <file>`. | Prints text files in bounded chunks; rejects directories. |
| `source/shell/shell.c` | Add `WRITE [-F] <path> <text>`. | Creates or overwrites text files only under `/workspace`; overwrite behavior is explicit. |
| `source/shell/shell.c` | Add `RM <path>` and `RM -R <path>`. | Deletes files under `/workspace`; recursive delete stays inside `/workspace` and rejects workspace root. |
| `source/shell/shell.c` | Add `CP` and `MV`. | File copy and file/directory move/rename stay inside `/workspace`. |
| `source/shell/shell.c` | Update `HELP`. | Shows current shell commands and marks BASIC/ASM expansion as later. |

### 2.2 Shell reliability gates

| File/Folder | Task | Acceptance Criteria |
| --- | --- | --- |
| `tools/adversarial_shell_smoke.py` | Keep nonsense/flood tests current with each new command. | Malformed commands, unsafe paths, long lines, binary-ish input, and command bursts keep returning to prompt. |
| `tools/workspace_shell_smoke.py` | Preserve existing file smoke. | Existing LS/WRITE/CP/MV/RM/RMDIR/RM -R flow passes. |
| `tools/no_basic_shell_smoke.py` | Preserve shell/runtime separation. | BASIC commands and legacy aliases return `UNKNOWN COMMAND`. |
| `tools/renew_full_smoke.py` | Preserve renewal smoke. | Written workspace marker is removed by `RENEW` and layout is recreated. |
| `main/storage.c` | Preserve broken-workspace boot path. | If workspace mount/layout fails, shell still starts and `RENEW` can rebuild workspace. |

### 2.3 Input boundary

| File/Folder | Task | Acceptance Criteria |
| --- | --- | --- |
| `main/input.h` | Define shell input event API. | Shell does not know whether input came from USB serial or BLE HID. |
| `main/input_serial.c` | Move current USB Serial/JTAG line input behind the input API. | PC terminal behavior is unchanged and all shell tests pass. |
| `main/input_ble_hid.c` | Add BLE HID keyboard backend after serial remains stable. | BLE pairing/report parsing is isolated from shell command handling. |
| `main/CMakeLists.txt` | Add new input files only when implemented. | Build remains link-complete. |

## 3. Deferred Milestone — ASM Capture and Storage Boundary

Goal: capture `ASM`/`ENDASM` in BASIC safely before any native execution.

### 2.1 Parsing & AST extraction

| File/Folder | Task | Acceptance Criteria |
| --- | --- | --- |
| `main/basic.h` | Add assembly block API declarations (`asm` parse/type definitions). | Header includes public prototypes used by both shell and BASIC run paths. |
| `main/basic.c` | Add parser branch for `ASM` block inside BASIC source and BASIC-only execution. | Given `ASM` ... `ENDASM` in a program, the block is captured exactly once and stored separately from BASIC text. |
| `main/storage.c` | Add dedicated write/read for assembly source files under `/ASM`. | `foo.bas` and `foo.asm` are independently retrievable with deterministic naming. |
| `source/shell/shell.c` | Accept command `asm` as standalone editor launcher when target is selected (if shell mode retains command parity). | Running `asm` with a valid BASIC/ASM file does not crash and returns meaningful errors. |

### 2.2 Unsupported-instruction reject path

| File/Folder | Task | Acceptance Criteria |
| --- | --- | --- |
| `main/basic.c` | Add line-precise validation for `CALLASM` usage syntax and for unsupported ASM instructions. | Error includes source line context and token that failed; phase 2 execution remains disabled. |
| `main/basic.c` or new `main/asm_parser.c` | Add whitelist of allowed opcodes for first pass (`add`, `addi`, `sub`, `and`, `or`, `xor`, `lw`, `sw`, `beq`, `jal`, `ret`). | Any non-whitelisted mnemonic exits with deterministic `ASM ERROR` and does not emit binary output. |
| `test/` | Add regression file list for positive/negative ASM-block cases (text fixture + expected parser output). | Each invalid instruction case fails before runtime execution. |

### 2.3 Independent assembler output verification

| File/Folder | Task | Acceptance Criteria |
| --- | --- | --- |
| `test/` | Add a compile-only script (Python or shell) for `.asm` fixtures. | Output binaries hash-map is stable for same input and host-agnostic. |
| `main/basic.c` | Emit assembler result object/metadata without executing it. | Stored output can be listed and reloaded after reboot; no crash on malformed input. |
| `README.md` and `docs/` | Document that assembler is capture-only in milestone 2A. | User-visible docs match shipped behavior. |

## 4. Deferred Milestone — Execution Boundary

Goal: `CALLASM()` and `.asm` command execution only after 2A passes.

| File/Folder | Task | Acceptance Criteria |
| --- | --- | --- |
| `main/basic.c` | Implement `CALLASM()` builtin and argument contract (`a0..a3`/`a0` return). | Calling native routine returns deterministic value and does not block shell. |
| `main/` (new `asm_runtime.c`, `asm_runtime.h`) | Add runtime loader + execution wrapper (IRAM path only if available). | Valid programs execute and return to READY without resource leak. |
| `main/basic.c` | Add timeout/guarded abort for runaway loops in assembly execution path. | Hang behavior is bounded by watchdog-safe recovery flow. |
| `source/shell/shell.c` | Add explicit `asm` / `reg` / `disasm` command stubs or explicit "not supported yet" message. | No ambiguous failures; every entry is deterministic and documented. |
| `test/` | Add smoke test using `CALLASM` in a BASIC program and standalone `.asm` compile+run case. | Both pass on target (or host harness if applicable). |

## 5. Deferred Milestone — Monitor Tooling

Goal: non-disruptive observability layer.

| File/Folder | Task | Acceptance Criteria |
| --- | --- | --- |
| `source/shell/shell.c` | Implement command handlers for `reg`, `mem`, `dump`, `disasm`, `step`, `break`. | Commands are discoverable via `help` and return predictable structured output. |
| `main/CMakeLists.txt` | Add any new module dependencies required by monitor tools. | Build remains reproducible and clean. |
| `docs/04_Shell_Reference.md` | Add monitor command reference and examples. | Reference matches behavior exactly. |

## 6. Definition of Done per milestone

For each milestone item above:

- [ ] Command and behavior are documented first.
- [ ] Unit/fixture coverage exists for success and failure path.
- [ ] Manual serial demonstration output is reproducible.
- [ ] Previous milestone demonstrations still execute.
- [ ] README and docs reflect what is currently shipped.

## 7. Suggested Working Sequence (recommended)

1. Sprint 002 micro Linux shell command set
2. Sprint 002 input boundary, PC terminal first
3. BLE HID keyboard backend
4. ASM capture boundary
5. ASM execution plumbing
6. Monitor tools
7. BASIC/library expansion only after each milestone stays demonstrable

## 8. Current executable sprint

Sprint 002 is complete. Use [`docs/SPRINT_002_TASK_LIST.md`](docs/SPRINT_002_TASK_LIST.md) as the completion and evidence record.

Related plan: [`docs/SPRINT_002_WORKSPACE_SHELL_AND_INPUT_PLAN.md`](docs/SPRINT_002_WORKSPACE_SHELL_AND_INPUT_PLAN.md) captures the recovery-partition, micro Linux workspace shell, PC terminal, and future BLE HID keyboard input boundary.

ASM capture remains documented in [`docs/SPRINT_001_PHASE2A_ASM_CAPTURE.md`](docs/SPRINT_001_PHASE2A_ASM_CAPTURE.md). It is the next candidate milestone after Sprint 002, but native execution stays blocked until a later guarded runtime sprint.
