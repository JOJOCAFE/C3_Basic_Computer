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
- Boot and protected recovery do not depend on workspace files. If `workspace_fs`
  is damaged, the shell still starts from the protected path and `RENEW` can
  rebuild workspace after two confirmations.
- Sprint 002 Micro UNIX-style workspace shell is complete and board-tested,
  using the local OpenC6 BIOS fork as a structure reference.
- Native RISC-V assembly capture is the next candidate milestone. Native
  execution remains blocked until a later guarded runtime sprint.
- BLE HID keyboard support has a compiled input boundary, but real keyboard
  pairing remains hardware-pending.

## Design snapshot

- Boot experience: `READY.` prompt after startup
- Workspace path: `/` with writable folders (`/basic`, `/asm`, `/bin`, `/config`,
  `/data`, `/temp`)
- Renewed workspace is constrained to `workspace_fs`; system storage remains separate.
- `RENEW` is protected behavior and formats only `workspace_fs`; there is no
  public `FORMAT` command in the shell sprint.
- Command style: C3-compatible uppercase commands now, with Unix-style aliases
  where they help (`DIR`/`LS`, `DELETE`/`RM`, `COPY`/`CP`, `MOVE`/`MV`)
- Language-first interaction: BASIC and assembly are the main maker interfaces
- Recoverability: system-protected runtime plus renewable user workspace

## Repository map

- `docs/` — architecture, language, shell, filesystem, commands, and roadmap
- `firmware/` — current firmware implementation
- `Old_version/tools/` — current build scripts and serial smoke tests
- `hardware/` — hardware-related notes/design artifacts
- `assets/`, `examples/`, `test/` — project assets, samples, and checks
- `Old_version/` — legacy/reference history and phase records
- `.codex/PROJECT_SKILL.md` — project skill contract (Manifesto + Design Principles)

## Build and flash

The stable launcher is in `Old_version/tools/idf53.sh` and expects ESP-IDF 5.3.x:

```bash
Old_version/tools/idf53.sh -C Old_version -B build-idf53 build
Old_version/tools/idf53.sh -C Old_version -B build-idf53 -p /dev/ttyACM0 flash
```

Common host-side checks:

```bash
python3 Old_version/tools/dir_delete_smoke.py --port /dev/ttyACM0
python3 Old_version/tools/load_list_run_smoke.py --port /dev/ttyACM0
python3 Old_version/tools/reboot_persistence_smoke.py --port /dev/ttyACM0
python3 Old_version/tools/workspace_shell_smoke.py --port /dev/ttyACM0
python3 Old_version/tools/renew_full_smoke.py --port /dev/ttyACM0
python3 Old_version/tools/adversarial_shell_smoke.py --port /dev/ttyACM0
```

## Documented command surface

Current shell core:
`HELP`, `PWD`, `DIR`, `LS`, `CD`, `MKDIR`, `CAT`, `WRITE`, `DELETE`, `RM`,
`COPY`, `CP`, `MOVE`, `MV`, `LOAD`, `SAVE`, `NEW`, `LIST`, `RUN`, `RENEW`

Later workspace/system/monitor targets:
`EDIT`, `BASIC`, `ASM`, `VERSION`, `MEMORY`, `DATE`, `TIME`, `DIAGNOSTICS`,
`REG`, `MEM`, `DUMP`, `DISASM`, `STEP`, `BREAK`

Only implemented commands should appear in firmware `HELP`.

## Strict implementation checklist

See [`docs/IMPLEMENTATION_CHECKLIST.md`](/home/jo/Codex/C3_Basic_Computer/docs/IMPLEMENTATION_CHECKLIST.md) for a folder/file-level execution plan and acceptance criteria.

Completed high-priority implementation plan:

[`docs/SPRINT_002_TASK_LIST.md`](/home/jo/Codex/C3_Basic_Computer/docs/SPRINT_002_TASK_LIST.md)

## Implementation plan (recommended next steps)

Sprint 002 shell-first work is complete. Resume in the order below.

1. **Completed – Micro UNIX-style workspace shell**
   - `PWD`, `CD`, `LS`/`DIR`, `MKDIR`, `CAT`, `WRITE`, `RM`/`DELETE`,
     `COPY`/`CP`, and `MOVE`/`MV` are implemented.
   - Keep all file commands constrained to `/workspace`.
   - Keep `FORMAT`, `BOOT`, `RAMBOOT`, `XIP`, `PXE`, and OTA out of this sprint.

2. **Completed – Input boundary**
   - PC terminal over USB Serial/JTAG is the active backend.
   - Shell input is behind an input service.
   - BLE HID keyboard backend boundary exists; real pairing waits for hardware.

3. **Next candidate – ASM capture boundary**
   - Parse and capture `ASM`/`ENDASM` blocks from BASIC safely.
   - Keep assembly source separate from BASIC source storage.
   - Validate assembler input before any execution work.

4. **Later – Runtime, monitor, and capability expansion**
   - Add `CALLASM()`, standalone `.asm` execution, and monitor commands only after
     capture/validation passes.
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
