# C3 BASIC COMPUTER Session Status

Date: 2026-06-27

## Completed Today

- Shell YMODEM transfer is implemented and board-tested: `DF`, `RECV [-F]`,
  and `SEND`.
- Guarded C3COM execution is implemented and board-tested:
  `RUN /bin/name.com [args...]`.
- C3COM runner validates header fields and CRC before copying code to
  executable RAM and jumping.
- Board smoke confirmed argv/stdout/`EXIT 0` and bad-CRC rejection.
- ESP32-C3 memory protection is disabled in project defaults so
  `MALLOC_CAP_EXEC` heap is available for native C3COM execution.
- First-slice `.com` programs must be position-independent flat code; no
  relocation loader exists yet.
- Tiny numbered BASIC runs from nano BASIC mode with `:run`.
- BASIC `:debug` step mode is implemented.
- Nano/BASIC source loading uses the 64 KiB editor buffer.
- BASIC validation errors are structured and line-anchored.
- BASIC GPIO/ADC typed hardware surface is implemented through
  `main/basic_hardware.*` and `source/hardware`.
- Project README, service READMEs, and current docs were synced to the
  implemented BASIC/nano/hardware behavior.
- Every `.c` and `.h` source file now ends with `//Keep Going.`.
- `.codex/PROJECT_SKILL.md` now records the source-footer rule.
- `Old_version/` was reviewed and removed because the active root/source docs
  now preserve the useful requirements without stale command/runtime behavior.
- Sprint 1 recovery docs were reviewed and migrated from legacy `Old_version/`
  paths to current root/source paths.

## Latest Commits

- `9a8a51e` Remove legacy tree and update Sprint 1 docs
- `6a21dca` Add session handoff
- `bcf3033` Require Keep Going source footer
- `46ea658` Sync docs with BASIC hardware services
- `0f72896` Add typed BASIC hardware service

## Current Branch State

- Branch: `main`
- Synced after final push.

## Known Unstaged Local Changes

Sprint 010 terminal service planning docs are ready to commit.

## Recommended Next Start

1. Start Sprint 010 from
   `docs/SPRINT_010_TERMINAL_SERVICE_AND_BASIC_TERM.md`.
2. Implement `/bin/term` as an output-only ANSI/VT100 terminal service.
3. Add BASIC `TERM "..."` as a safe bridge to `/bin/term`.
4. Then resume Sprint 009 from `docs/SPRINT_009_ASM_NANO_MODE_TASK_LIST.md`.
5. Implement `ASM /asm/name.asm` as nano ASM mode with text validation only.
6. Resume ASM capture as a non-execution milestone after ASM editor mode is
   stable and board-tested.

Keep Going.
