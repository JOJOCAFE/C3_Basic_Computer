# C3 BASIC COMPUTER Implementation Checklist (Strict)

This checklist is tied to the code that currently exists in `Old_version/`.
Use it as the execution plan so milestones remain auditable.

## 0. Baseline State (Do not change before passing)

- [x] `Old_version/main/main.c` boots and calls `shell_run()`.
- [x] `Old_version/main/storage.c` mounts split LittleFS partitions (`system_fs`, `workspace_fs`) and creates canonical workspace directories.
- [x] `Old_version/main/shell.c` offers `HELP`, `DIR`, `LOAD`, `SAVE`, `DELETE`, `NEW`, `LIST`, `RUN`.
- [x] `Old_version/main/basic.c` supports Phase 1 BASIC statements/functions.
- [x] `Old_version/README.md` and `Old_version/TASK_LIST.md` match current scope.

## 1. Move Implementation Surface to a Stable Workspace

Goal: keep new work out of legacy build artifacts and in versioned sources.

| File/Folder | Task | Acceptance Criteria |
| --- | --- | --- |
| `Old_version/main/main.c` | Confirm boot path and storage init are unchanged before refactors. | Cold boot still shows READY prompt and shell banner on `/dev/ttyACM0` or PC terminal. |
| `Old_version/main/CMakeLists.txt` | Keep source list in sync with new modules. | Build is link-complete after adding/removing files. |
| `tools/` (root) | Mirror required tooling from `Old_version/tools` for consistency. | `idf53.sh`, `dir_delete_smoke.py`, `load_list_run_smoke.py`, `reboot_persistence_smoke.py` exist and run with port argument. |
| `docs/` | Mark progress status in implementation docs for files you will edit. | Each touched `.md` has matching behavior in code. |

## 2. Milestone 2A — ASM Capture and Storage Boundary

Goal: capture `ASM`/`ENDASM` in BASIC safely before any native execution.

### 2.1 Parsing & AST extraction

| File/Folder | Task | Acceptance Criteria |
| --- | --- | --- |
| `Old_version/main/basic.h` | Add assembly block API declarations (`asm` parse/type definitions). | Header includes public prototypes used by both shell and BASIC run paths. |
| `Old_version/main/basic.c` | Add parser branch for `ASM` block inside BASIC source and BASIC-only execution. | Given `ASM` ... `ENDASM` in a program, the block is captured exactly once and stored separately from BASIC text. |
| `Old_version/main/storage.c` | Add dedicated write/read for assembly source files under `/ASM`. | `foo.bas` and `foo.asm` are independently retrievable with deterministic naming. |
| `Old_version/main/shell.c` | Accept command `asm` as standalone editor launcher when target is selected (if shell mode retains command parity). | Running `asm` with a valid BASIC/ASM file does not crash and returns meaningful errors. |

### 2.2 Unsupported-instruction reject path

| File/Folder | Task | Acceptance Criteria |
| --- | --- | --- |
| `Old_version/main/basic.c` | Add line-precise validation for `CALLASM` usage syntax and for unsupported ASM instructions. | Error includes source line context and token that failed; phase 2 execution remains disabled. |
| `Old_version/main/basic.c` or new `Old_version/main/asm_parser.c` | Add whitelist of allowed opcodes for first pass (`add`, `addi`, `sub`, `and`, `or`, `xor`, `lw`, `sw`, `beq`, `jal`, `ret`). | Any non-whitelisted mnemonic exits with deterministic `ASM ERROR` and does not emit binary output. |
| `test/` | Add regression file list for positive/negative ASM-block cases (text fixture + expected parser output). | Each invalid instruction case fails before runtime execution. |

### 2.3 Independent assembler output verification

| File/Folder | Task | Acceptance Criteria |
| --- | --- | --- |
| `test/` | Add a compile-only script (Python or shell) for `.asm` fixtures. | Output binaries hash-map is stable for same input and host-agnostic. |
| `Old_version/main/basic.c` | Emit assembler result object/metadata without executing it. | Stored output can be listed and reloaded after reboot; no crash on malformed input. |
| `Old_version/README.md` | Document that assembler is capture-only in milestone 2A. | User-visible docs match shipped behavior. |

## 3. Milestone 2B — Execution Boundary

Goal: `CALLASM()` and `.asm` command execution only after 2A passes.

| File/Folder | Task | Acceptance Criteria |
| --- | --- | --- |
| `Old_version/main/basic.c` | Implement `CALLASM()` builtin and argument contract (`a0..a3`/`a0` return). | Calling native routine returns deterministic value and does not block shell. |
| `Old_version/main/` (new `asm_runtime.c`, `asm_runtime.h`) | Add runtime loader + execution wrapper (IRAM path only if available). | Valid programs execute and return to READY without resource leak. |
| `Old_version/main/basic.c` | Add timeout/guarded abort for runaway loops in assembly execution path. | Hang behavior is bounded by watchdog-safe recovery flow. |
| `Old_version/main/shell.c` | Add explicit `asm` / `reg` / `disasm` command stubs or explicit "not supported yet" message. | No ambiguous failures; every entry is deterministic and documented. |
| `test/` | Add smoke test using `CALLASM` in a BASIC program and standalone `.asm` compile+run case. | Both pass on target (or host harness if applicable). |

## 4. Milestone 3 — Monitor Tooling

Goal: non-disruptive observability layer.

| File/Folder | Task | Acceptance Criteria |
| --- | --- | --- |
| `Old_version/main/shell.c` | Implement command handlers for `reg`, `mem`, `dump`, `disasm`, `step`, `break`. | Commands are discoverable via `help` and return predictable structured output. |
| `Old_version/main/CMakeLists.txt` | Add any new module dependencies required by monitor tools. | Build remains reproducible and clean. |
| `docs/04_Shell_Reference.md` | Add monitor command reference and examples. | Reference matches behavior exactly. |

## 5. Definition of Done per milestone

For each milestone item above:

- [ ] Command and behavior are documented first.
- [ ] Unit/fixture coverage exists for success and failure path.
- [ ] Manual serial demonstration output is reproducible.
- [ ] Previous milestone demonstrations still execute.
- [ ] README and docs reflect what is currently shipped.

## 6. Suggested Working Sequence (recommended)

1. Phase 2A parser isolation (2.1 → 2.2 → 2.3)  
2. Phase 2B execution plumbing (2.3 artifact reused)  
3. Monitor tools (Milestone 3)  
4. Expand shell and library features only after each milestone stays demonstrable

## 7. Current executable sprint

Start with [`docs/SPRINT_001_PHASE2A_ASM_CAPTURE.md`](docs/SPRINT_001_PHASE2A_ASM_CAPTURE.md) for a strict 12-step execution list.
