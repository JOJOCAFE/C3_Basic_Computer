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
- Shell filename wildcard support is implemented for `LS`, `CAT`, `RM`, `CP`,
  and `MV`. Supported patterns include `*.*`, `*.bas`, and `xx.*`.
- `CP` and `MV` wildcard sources require an existing destination directory and
  keep each matching filename.
- `tools/workspace_shell_smoke.py` now covers wildcard `LS`, `CAT`, `RM`,
  `CP`, and `MV` behavior.
- Host verification passed for the wildcard change:
  `python3 -m py_compile tools/workspace_shell_smoke.py`,
  `git diff --check`, and `tools/idf53.sh -B build-c3-root build`.
- Board verification passed on `/dev/ttyACM0`:
  `tools/idf53.sh -B build-c3-root -p /dev/ttyACM0 flash` and
  `python3 tools/workspace_shell_smoke.py --port /dev/ttyACM0`.
- Main task stack default is now 8192 bytes so wildcard file operations have
  enough stack headroom on ESP32-C3.

## Latest Commits

- Current `HEAD` Add shell wildcard file commands
- `9427712` Plan terminal service for BASIC text UI
- `e92cfca` Add shell YMODEM transfer and C3COM runner
- `f7b5f5d` Update final session status
- `9a8a51e` Remove legacy tree and update Sprint 1 docs

## Current Branch State

- Branch: `main`
- Tracks `origin/main`.
- Current wildcard changes are committed locally and ready to push.

## Known Unstaged Local Changes

- None expected after the wildcard test commit is amended.

## Board Verification

- Latest board-tested commands:
  ```bash
  tools/idf53.sh -B build-c3-root -p /dev/ttyACM0 flash
  python3 tools/workspace_shell_smoke.py --port /dev/ttyACM0
  ```

## Recommended Next Start

1. Push the local wildcard shell commit if remote sync is desired.
2. Start Sprint 010 from
   `docs/SPRINT_010_TERMINAL_SERVICE_AND_BASIC_TERM.md`.
3. Implement `/bin/term` as an output-only ANSI/VT100 terminal service.
4. Add BASIC `TERM "..."` as a safe bridge to `/bin/term`.
5. Then resume Sprint 009 from `docs/SPRINT_009_ASM_NANO_MODE_TASK_LIST.md`.
6. Implement `ASM /asm/name.asm` as nano ASM mode with text validation only.
7. Resume ASM capture as a non-execution milestone after ASM editor mode is
   stable and board-tested.

Keep Going.
