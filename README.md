# C3 BASIC COMPUTER

Educational, terminal-first computer system for the ESP32-C3 with a focus on
maker-friendly programming, recoverability, and incremental implementation.

This project is documented as a reference implementation.  
Core ideas:
- Computer-first architecture (not firmware-first)
- Simple, predictable shell-driven workflow
- Tiny text-based system designed for learning and hacking

Current implementation context:
- Phase 1 is now oriented around recoverability.
- `Old_version` flash partitioning is split into two LittleFS areas:
  - `system_fs` (read-mostly runtime boundary)
  - `workspace_fs` (renewable user workspace)
- Current milestone progression still targets Phase 2: native RISC-V assembly support.

## Design snapshot

- Boot experience: `READY.` prompt after startup
- Workspace path: `/` with writable folders (`/basic`, `/asm`, `/bin`, `/config`,
  `/data`, `/temp`)
- Renewed workspace is constrained to `workspace_fs`; system storage remains separate.
- Command style: lowercase, Unix-like, compact messages
- Language-first interaction: BASIC and assembly are the main maker interfaces
- Recoverability: system-protected runtime plus renewable user workspace

## Repository map

- `docs/` — architecture, language, shell, filesystem, commands, and roadmap
- `firmware/` — current firmware implementation
- `tools/` — build scripts and serial smoke tests
- `hardware/` — hardware-related notes/design artifacts
- `assets/`, `examples/`, `test/` — project assets, samples, and checks
- `Old_version/` — legacy/reference history and phase records
- `.codex/PROJECT_SKILL.md` — project skill contract (Manifesto + Design Principles)

## Build and flash

The stable launcher is in `tools/idf53.sh` and expects ESP-IDF 5.3.x:

```bash
tools/idf53.sh -B build-idf53 build
tools/idf53.sh -B build-idf53 -p /dev/ttyACM0 flash
```

Common host-side checks:

```bash
python3 tools/dir_delete_smoke.py --port /dev/ttyACM0
python3 tools/load_list_run_smoke.py --port /dev/ttyACM0
python3 tools/reboot_persistence_smoke.py --port /dev/ttyACM0
```

## Documented command surface

Shell core:
`help`, `pwd`, `ls`, `cd`, `cat`, `cp`, `mv`, `rm`, `mkdir`, `rmdir`

Workspace:
`edit`, `run`, `basic`, `asm`

System:
`version`, `memory`, `date`, `time`, `renew`, `update`, `diagnostics`

Monitor:
`reg`, `mem`, `dump`, `disasm`, `step`, `break`

## Strict implementation checklist

See [`docs/IMPLEMENTATION_CHECKLIST.md`](/home/jo/Codex/C3_Basic_Computer/docs/IMPLEMENTATION_CHECKLIST.md) for a folder/file-level execution plan and acceptance criteria.

Current high-priority implementation plan:

[`docs/PHASE1_FIX_LIST.md`](/home/jo/Codex/C3_Basic_Computer/docs/PHASE1_FIX_LIST.md)

## Implementation plan (recommended next steps)

Follow the documented milestones in `docs/IMPLEMENTATION_PLAN.md` and `Old_version/TASK_LIST.md`.

1. **Phase 2 – Assembly capture boundary**
   - Parse and capture `ASM`/`ENDASM` blocks from BASIC safely.
   - Keep assembly source separate from BASIC source storage.
   - Add line-specific errors for unsupported instructions.
   - Validate assembler output in isolation before enabling execution.

2. **Phase 2 – Runtime execution boundary**
   - Add `CALLASM()` and standalone `.asm` execution flow.
   - Execute in a validated sandbox path (IRAM path once assembly correctness is proven).
   - Add minimal diagnostics for run state and failure causes.

3. **Phase 3 – Monitor tools**
   - Implement `reg`, `mem`, `disasm`, `step`, `break`, `dump`.
   - Keep monitor features independent and non-disruptive to shell/BASIC flow.

4. **Phase 4+ – Capability expansion**
   - Add graphics, sound, GPIO/motion, then standalone UX hardware integrations as separate milestones.

## UX and behavior goals

- `READY.` is the standard idle state.
- Errors explain what happened and where it happened; avoid numeric codes.
- Warnings are reserved for potentially destructive actions.
- Success returns to `READY.` with minimal noise.
- Keep the system small, understandable, and transparent.

## Compatibility intent

Architecture and behavior are designed to stay consistent across the C-Series
BASIC COMPUTER family where practical (C3/C5/C6/P4).
