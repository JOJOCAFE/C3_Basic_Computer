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
- Sprint 010 T10-T2 terminal helper layer is implemented in `source/terminal`
  with callback-based output and exact ANSI/VT100 escape mappings.
- Sprint 010 T10-T3 `/bin/term` service is implemented and registered in
  `/bin list`.
- Focused board verification passed on `/dev/ttyACM0` for `/bin/term clear`,
  `home`, `goto`, `color`, `reset`, `hide-cursor`, `show-cursor`, bad color
  rejection, `/bin list` containing `/bin/term`, and `HELP` not listing
  `/bin/term`.
- Sprint 010 T10-T4 through T10-T8 are complete: BASIC `TERM "..."` uses a
  dedicated service kind, bridges only to whitelisted `/bin/term` operations,
  includes `examples/basic/term_demo.bas`, and is covered by
  `tools/bin_term_smoke.py`.
- Sprint 011 is complete: `EDIT`, `/bin/nano`, and `BASIC` can start without a
  filename. Untitled text buffers save to `/data/untitled-N.txt`; untitled BASIC
  buffers save to `/basic/untitled-N.bas`; clean `:q` creates no file and dirty
  `:q` requires save or discard.

## Latest Commits

- `79d3f0b` Start Sprint 010 terminal service docs
- `bf117aa` Add shell wildcard file commands
- `9427712` Plan terminal service for BASIC text UI
- `e92cfca` Add shell YMODEM transfer and C3COM runner
- `f7b5f5d` Update final session status

## Current Branch State

- Branch: `main`
- Tracks `origin/main`.
- Sprint 010 `/bin/term` plus BASIC `TERM` and Sprint 011 untitled editor
  changes are implemented, board-tested, and ready to commit.

## Known Unstaged Local Changes

- Sprint 010 BASIC `TERM`, Sprint 011 untitled editor launch, docs, smokes, and
  this handoff update.

## Board Verification

- Latest board-tested commands:
  ```bash
  tools/idf53.sh -B build-c3-root -p /dev/ttyACM0 flash
  ```
- Latest focused `/bin/term` smoke checks:
  - `/bin list` includes `/bin/term`.
  - `HELP` does not include `/bin/term`.
  - `/bin/term clear` emits `ESC[2J ESC[H`.
  - `/bin/term home` emits `ESC[H`.
  - `/bin/term goto -r 3 -c 4` emits `ESC[3;4H`.
  - `/bin/term color -f 2` emits `ESC[32m`.
  - `/bin/term color -f 2 -b 4` emits `ESC[32;44m`.
  - `/bin/term reset` emits `ESC[0m`.
  - `/bin/term hide-cursor` emits `ESC[?25l`.
  - `/bin/term show-cursor` emits `ESC[?25h`.
  - `/bin/term color -f 8` returns `Bad argument`.
- Latest focused BASIC `TERM` smoke checks:
  - `TERM "clear"` emits `ESC[2J ESC[H`.
  - `TERM "goto -r 3 -c 4"` emits `ESC[3;4H`.
  - `TERM "color -f 2"` emits `ESC[32m`.
  - `TERM "reset"` emits `ESC[0m`.
  - `TERM "rm -r /"` is blocked with `BASIC TERM command blocked`.
  - Overlong/malformed direct and BASIC TERM commands are rejected.
- Latest untitled editor smoke checks:
  - `EDIT`, dirty text, `:wq` creates the first free `/data/untitled-N.txt`.
  - `/bin/nano`, dirty text, `:wq` creates the first free `/data/untitled-N.txt`.
  - `BASIC`, numbered source, `:wq` creates the first free `/basic/untitled-N.bas`.
  - Clean untitled `:q` does not create a file.
  - Dirty untitled `:q` refuses and `:q!` discards.

## Recommended Next Start

1. Commit and push Sprint 010 BASIC `TERM` plus Sprint 011 untitled editor
   launch if the current diff is accepted.
2. Resume Sprint 009 from `docs/SPRINT_009_ASM_NANO_MODE_TASK_LIST.md`.
3. Implement `ASM /asm/name.asm` as nano ASM mode with text validation only.
4. Resume ASM capture as a non-execution milestone after ASM editor mode is
   stable and board-tested.

Keep Going.
