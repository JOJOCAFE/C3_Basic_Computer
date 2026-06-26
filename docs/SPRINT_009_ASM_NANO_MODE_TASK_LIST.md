# Sprint 009 Task List: ASM Nano Mode, Text Validation Only

Status: Planned

Goal: add `ASM /asm/name.asm` as a nano editor mode for assembly text. This
sprint validates and saves assembly source only. It must not assemble, emit
binary, call native code, or expose CPU monitor/runtime behavior.

This sprint comes before the older Phase 2A `ASM`/`ENDASM` capture work. The
current architecture routes language-specific editing through nano plugins, so
standalone ASM editor mode should become stable before BASIC-side ASM capture is
resumed.

## Current Starting Point

- `EDITOR_MODE_ASM` exists in `source/editor/editor_service.h`.
- `source/editor/editor_service.c` currently rejects ASM mode with
  `Language plugin not available`.
- `source/bin/bin_registry.c` registers `/bin/nano`, `EDIT`, and `BASIC`, but
  does not register `ASM`.
- `source/bin/bin_nano.c` has shared editor launcher plumbing for text and
  BASIC modes.
- `/workspace/asm` is created by the workspace layout.
- Native ASM execution remains blocked by design.

## Non-Goals

- No assembler backend.
- No object file or binary sidecar.
- No `CALLASM()`.
- No CPU register, memory, disassembly, breakpoint, or step monitor commands.
- No native execution from the editor, shell, BASIC, or `/bin` services.
- No BASIC `ASM`/`ENDASM` capture in this sprint.

## Task List

### A9-T1 - Freeze ASM editor contract

- [ ] Update:
  - `README.md`
  - `docs/04_Shell_Reference.md`
  - `docs/SPRINT_004_NANO_EDITOR_SERVICE.md`
  - `docs/COMPLIANCE_MATRIX.md`
- [ ] Record `ASM /asm/name.asm` as an editor launcher only.
- [ ] State that ASM save validation is text-only.
- [ ] State that assembly, binary generation, and execution remain blocked.
- [ ] Pass criteria:
  - Docs distinguish ASM editor mode from ASM runtime.
  - Docs do not claim `ASM` is in firmware `HELP` unless the implementation
    has been added.
  - The old Phase 2A ASM-capture doc is clearly a later follow-on.

### A9-T2 - Add ASM path policy

- [ ] File: `source/editor/editor_service.c`
- [ ] Accept only `/asm/*.asm` for `EDITOR_MODE_ASM`.
- [ ] Keep `EDIT /data/*.txt` and `BASIC /basic/*.bas` behavior unchanged.
- [ ] Reject path traversal with `..`.
- [ ] Reject wrong extensions and wrong top-level folders with a clear message:
  `Bad editor path. Use /asm/name.asm`
- [ ] Pass criteria:
  - `ASM /asm/test.asm` reaches nano ASM mode.
  - `ASM /data/test.txt` is rejected.
  - `ASM /basic/test.bas` is rejected.
  - `ASM ../x.asm` is rejected.

### A9-T3 - Register ASM shell alias

- [ ] File: `source/bin/bin_nano.c`
- [ ] File: `source/bin/bin_registry.c`
- [ ] Add `bin_asm_exec()` using `EDITOR_MODE_ASM`.
- [ ] Register `ASM` as a removable nano-backed service alias.
- [ ] Do not add a separate assembler service.
- [ ] Pass criteria:
  - `ASM /asm/test.asm` dispatches to the nano editor service.
  - `/bin list` includes the ASM editor service only if the registry model
    requires a visible service entry; otherwise document why it remains an alias.
  - Existing `EDIT` and `BASIC` entry points still work.

### A9-T4 - Add ASM text validator

- [ ] File: `source/editor/editor_service.c` or a new editor plugin file.
- [ ] Validate on `:w` and `:wq`.
- [ ] Allow:
  - blank lines
  - comments beginning with `;` or `#`
  - labels shaped as `name:`
  - simple instruction-looking lines for text capture
- [ ] Reject:
  - unsupported binary/control characters
  - overlong source lines beyond a documented bound
  - malformed labels
  - lines that look like editor commands but are not entered as commands
- [ ] Do not validate real RISC-V opcode semantics in this task.
- [ ] Pass criteria:
  - Valid ASM text saves.
  - Invalid label shape reports a line-anchored error.
  - The editor buffer remains intact after validation errors.

### A9-T5 - Keep run/debug blocked in ASM mode

- [ ] File: `source/editor/editor_service.c`
- [ ] Ensure `:run` in ASM mode prints a clear blocked message.
- [ ] Ensure `:debug` in ASM mode prints a clear blocked message.
- [ ] Message should say execution is not available in this sprint.
- [ ] Pass criteria:
  - `:run` does not assemble or execute.
  - `:debug` does not enter CPU or BASIC debug behavior.
  - The user returns to the `edit>` prompt after the message.

### A9-T6 - Add host/serial smoke coverage

- [ ] Add `tools/asm_editor_smoke.py`.
- [ ] Cover:
  - opening `ASM /asm/test.asm`
  - appending valid ASM-like text
  - saving with `:w`
  - printing with `:p`
  - rejecting malformed label text
  - rejecting `:run` / `:debug` execution
  - quitting cleanly
- [ ] Pass criteria:
  - Smoke script can run against `/dev/ttyACM0`.
  - It fails shell-visibly on missing prompts or wrong messages.
  - Existing workspace, nano, BASIC, and pipe smokes still pass.

### A9-T7 - Build and board verification

- [ ] Build:
  ```bash
  tools/idf53.sh -B build-c3-root build
  ```
- [ ] Board smoke, when `/dev/ttyACM0` is visible:
  ```bash
  tools/idf53.sh -B build-c3-root -p /dev/ttyACM0 flash
  python3 tools/workspace_shell_smoke.py --port /dev/ttyACM0
  python3 tools/nano_editor_smoke.py --port /dev/ttyACM0 --timeout 25
  python3 tools/asm_editor_smoke.py --port /dev/ttyACM0 --timeout 25
  python3 tools/bin_pipe_smoke.py --port /dev/ttyACM0
  ```
- [ ] Pass criteria:
  - Build succeeds.
  - `ASM /asm/test.asm` is board-tested.
  - Text save/load works after reboot or shell restart if the smoke includes it.
  - No BASIC, shell, `/bin`, or recovery regressions are observed.

### A9-T8 - Completion docs and handoff

- [ ] Update:
  - `README.md`
  - `SESSION_STATUS.md`
  - `.codex/PROJECT_SKILL.md`
- [ ] Record board evidence and any blocked hardware checks.
- [ ] Mark this sprint complete only after build plus smoke evidence exists.
- [ ] Set the next candidate milestone to guarded ASM capture after ASM editor
  mode is stable.
- [ ] Pass criteria:
  - The repo has one clear next task after Sprint 009.
  - Docs still state that native ASM execution remains blocked.

## Execution Order

1. A7-T1
2. A7-T2
3. A7-T3
4. A7-T4
5. A7-T5
6. A7-T6
7. A7-T7
8. A7-T8

Keep Going.
