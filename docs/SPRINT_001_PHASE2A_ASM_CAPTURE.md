# Sprint 001 — Phase 2A: ASM Capture (Paused, execution blocked)

Status: Deferred; next candidate after completed Sprint 002 shell-first work.

Scope when resumed: implement `ASM` block capture and deterministic validation in the active root codebase, without enabling native execution.

Execution remains disabled until a later ASM/runtime sprint. Sprint 002 has
completed the micro Linux workspace shell and input boundary.

Design reference: OpenC6 BIOS, forked at <https://github.com/JOJOCAFE/openc6-bios>, is useful now for shell structure only and later as a possible reference for a small host-to-native-code ABI jump table and serial payload handoff. Do not import that runtime model into Phase 2A. This sprint only captures, stores, and validates assembly source.

This sprint is intentionally granular: one checkpoint at a time, each with clear pass criteria.

## Task 1 — Baseline hardening

- [ ] Rename implementation path target in docs to root `main/` and `source/shell/` in all new edits.
- [ ] Add note at top of sprint: `asm execution disabled until a later guarded runtime sprint`.
- [ ] Pass criteria:
  - The repository documents current status as “ASM capture deferred after completed Sprint 002 shell-first work”.
  - No functional behavior changed by documentation.

## Task 2 — Expose assembly metadata in BASIC API

- [ ] File: `main/basic.h`
- [ ] Add:
  - ASM block record type (source string + line range + status).
  - Program field for captured ASM source handle.
  - Public functions:
    - `basic_get_last_asm_block(...)` (or equivalent accessor)
    - `basic_store_asm_block(...)`
- [ ] Pass criteria:
  - Header compiles with no unresolved type errors from `basic.c`.

## Task 3 — Track ASM blocks in runtime program model

- [ ] File: `main/basic.c`
- [ ] Add persistent storage for one ASM block per BASIC program:
  - start line
  - end line
  - raw source body
- [ ] Pass criteria:
  - Loading an existing BASIC file with no ASM block preserves behavior.
  - Storing BASIC lines no longer corrupts ASM body.

## Task 4 — Parse `ASM` keyword line in program entry

- [ ] File: `main/basic.c`
- [ ] In `basic_store_line`, detect `ASM` line and switch line consumption mode.
- [ ] Pass criteria:
  - Entering `100 ASM` then lines, then `ENDASM` stores only raw ASM source, not as normal statement text.
  - `LIST` output excludes ASM body from BASIC execution stream.

## Task 5 — Parse `ENDASM` terminal token

- [ ] File: `main/basic.c`
- [ ] Validate proper termination for capture mode:
  - `ENDASM` required.
  - `ENDASM` outside capture returns line-specific error.
- [ ] Pass criteria:
  - Invalid nesting is rejected with explicit error path.
  - No crashes on malformed block ordering.

## Task 6 — Add ASM block file path policy

- [ ] File: `main/storage.h` and `main/storage.c`
- [ ] Add resolver for `storage` ASM artifacts:
  - BASIC sidecar naming rule (for example `BASIC/<name>.bas` + `ASM/<name>.asm`).
- [ ] Pass criteria:
  - No path traversal via `..` accepted.
  - Empty or malformed name rejected safely.

## Task 7 — Save/load/store ASM companion files separately

- [ ] File: `main/basic.c`
- [ ] Add dedicated save/load helpers:
  - save asm body to `/ASM` path
  - load asm body with BASIC program
- [ ] Pass criteria:
  - `SAVE` persists both `.bas` and `.asm` (if block exists).
  - `LOAD` restores both and keeps line numbers stable.

## Task 8 — Add assembler validation pass (no execution)

- [ ] File: `main/basic.c` or new `main/asm_validate.c`
- [ ] Add deterministic tokenizer+validator pass that:
  - accepts a whitelist of opcodes first.
  - rejects others with reason.
- [ ] Pass criteria:
  - Invalid instruction fails at edit/load time with a clear reason.
  - Valid block reaches “validated” state.

## Task 9 — Line-anchored error reporting

- [ ] File: `source/shell/shell.c` and `main/basic.c`
- [ ] Include line number context on validation failures.
- [ ] Pass criteria:
  - Error format includes:
    - BASIC line number
    - failing asm line
    - token text
  - Error message always stays plain text and short.

## Task 10 — Add parser-only regression fixtures

- [ ] File: `test/` (add new test inputs + expected outputs)
- [ ] Add at least:
  - one valid capture case
  - one missing `ENDASM`
  - one unknown opcode
- [ ] Pass criteria:
  - Fixture expectations are deterministic and documented.
  - At least 1 positive and 2 negative cases are present.

## Task 11 — CLI/tool visibility for asm capture status

- [ ] File: `source/shell/shell.c`
- [ ] Update `HELP` output to include `asm` as phase-2 capture feature availability.
- [ ] Pass criteria:
  - Running `help` lists the new assembly capture capability.
  - Message clearly says execution remains disabled this sprint.

## Task 12 — Sprint completion gate

- [ ] Update docs:
  - `README.md`
  - `docs/IMPLEMENTATION_CHECKLIST.md`
  - `docs/IMPLEMENTATION_PLAN.md` (if needed)
- [ ] Pass criteria:
  - New status says:
    - ASM capture deferred until this sprint is resumed
    - no native execution yet
  - Existing milestones 0–1 behavior unchanged.

## Execution order (do not reorder)

1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10 → 11 → 12
