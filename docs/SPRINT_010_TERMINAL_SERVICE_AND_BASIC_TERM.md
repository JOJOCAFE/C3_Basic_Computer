# Sprint 010 Task List: Terminal Service and BASIC TERM

Status: Complete; T10-T1 through T10-T8 implemented and board-smoked.

Goal: add a small terminal-control service so BASIC programs can build simple
text UIs without embedding ANSI escape strings directly. This is not a curses
library. It is an output-only VT100/ANSI helper exposed as `/bin/term` and a
safe BASIC `TERM "..."` bridge.

## Command Contract

```text
/bin/term clear
/bin/term home
/bin/term goto -r <row> -c <col>
/bin/term color -f <0-7> [-b <0-7>]
/bin/term reset
/bin/term hide-cursor
/bin/term show-cursor
```

BASIC contract:

```basic
10 TERM "clear"
20 TERM "goto -r 5 -c 10"
30 PRINT "HELLO"
40 TERM "color -f 2"
50 PRINT "GREEN"
60 TERM "reset"
```

Rows and columns are 1-based. The first slice is output-only: no raw key input,
mouse, alternate screen, scroll regions, screen buffer, or full curses API.

## ANSI/VT100 Mapping

The service emits fixed ASCII escape sequences:

```text
clear        ESC[2J ESC[H
home         ESC[H
goto R C     ESC[{R};{C}H
color F      ESC[3{F}m
color F B    ESC[3{F};4{B}m
reset        ESC[0m
hide-cursor  ESC[?25l
show-cursor  ESC[?25h
```

Use byte `0x1B` for `ESC`. Do not depend on `termcap`, `terminfo`, curses, or
ncurses in this sprint.

## Non-Goals

- No ncurses dependency.
- No raw-key input or `GETKEY`.
- No full-screen editor rewrite.
- No terminal capability database.
- No shell built-in command.
- No BASIC direct ANSI string requirement.

## Task List

### T10-T1 - Freeze terminal service contract

- [x] Update:
  - `README.md`
  - `docs/04_Shell_Reference.md`
  - `docs/06_BASIC_Language.md`
  - `source/bin/README.md`
  - `source/hardware/README.md` not changed; no hardware cross-reference is
    needed for this terminal service.
- [x] Document `/bin/term` as a `/bin` service, not a shell built-in.
- [x] Document BASIC `TERM "..."` as a safe output-only service bridge.
- [x] Document that this is ANSI/VT100 helper behavior, not curses/ncurses.
- [x] Pass criteria:
  - Docs do not claim raw keyboard or curses support.
  - Docs explain that `HELP` should not list `/bin/term`.

### T10-T2 - Add terminal helper layer

- [x] Add `source/terminal/`.
- [x] Add `terminal.h` and `terminal.c`.
- [x] Implement:
  - `terminal_clear()`
  - `terminal_home()`
  - `terminal_goto(row, col)`
  - `terminal_color(fg, bg, has_bg)`
  - `terminal_reset()`
  - `terminal_hide_cursor()`
  - `terminal_show_cursor()`
- [x] Use a provided write callback so the layer can write to shell/BASIC I/O.
- [x] Keep all output ASCII escape sequences.
- [x] Use the exact ANSI/VT100 mapping in this task document.
- [x] Pass criteria:
  - Helper layer compiles in root firmware.
  - No direct USB Serial/JTAG dependency inside the helper layer.

### T10-T3 - Add `/bin/term` service

- [x] Add `source/bin/bin_term.c`.
- [x] Update `source/bin/c3_bin_sources.cmake`.
- [x] Update `source/bin/bin_internal.h`.
- [x] Register service in `source/bin/bin_registry.c`.
- [x] Implement argument parsing for:
  - `clear`
  - `home`
  - `goto -r <row> -c <col>`
  - `color -f <0-7> [-b <0-7>]`
  - `reset`
  - `hide-cursor`
  - `show-cursor`
- [x] Reject bad rows, columns, colors, unknown flags, and extra args with a
  clear usage or `Bad argument` message.
- [x] Pass criteria:
  - `/bin/term clear` clears a compatible terminal.
  - `/bin/term goto -r 5 -c 10` positions following output.
  - `/bin/term color -f 2` changes following text color.
  - `/bin list` includes `term`.
  - Shell `HELP` remains unchanged.

### T10-T4 - Add BASIC `TERM` bridge

- [x] File: `main/basic.c`.
- [x] Add `TERM "..."` statement.
- [x] Route it through the existing BASIC service execution path with a new
  service kind, not through unrestricted shell execution.
- [x] File: `source/editor/editor_service.c`.
- [x] Allow only the `/bin/term` command family for BASIC `TERM`.
- [x] Keep `SHELL` and `HARDWARE` bridge behavior unchanged.
- [x] Pass criteria:
  - `TERM "clear"` works from BASIC.
  - `TERM "goto -r 5 -c 10"` works from BASIC.
  - `TERM "reset"` works from BASIC.
  - `TERM "rm -r /"` and unrelated service strings are rejected.

### T10-T5 - Add terminal examples

- [x] Add `examples/basic/term_demo.bas` or equivalent repo-local example path.
- [x] Demonstrate:
  - clear screen
  - title placement
  - colored status line
  - reset before program end
- [x] Keep example inside currently supported BASIC syntax.
- [x] Pass criteria:
  - Example loads in BASIC nano mode.
  - Example avoids unsupported strings, arrays, or unnumbered BASIC.

### T10-T6 - Add smoke tests

- [x] Add `tools/bin_term_smoke.py`.
- [x] Add or extend BASIC smoke coverage for `TERM`.
- [x] Cover:
  - `/bin/term clear`
  - `/bin/term home`
  - `/bin/term goto -r 3 -c 4`
  - `/bin/term color -f 2`
  - `/bin/term reset`
  - BASIC `TERM "clear"`
  - BASIC `TERM "goto -r 3 -c 4"`
- [x] For terminal escape validation, assert raw output contains the expected
  ANSI byte sequences.
- [x] Pass criteria:
  - Smoke fails shell-visibly on wrong escape sequences.
  - Existing workspace, BASIC, and `/bin` smokes still pass.

### T10-T7 - Build and board verification

- [x] Compile Python smoke tools:
  ```bash
  python3 -m py_compile tools/bin_term_smoke.py tools/nano_editor_smoke.py tools/basic_editor_smoke.py
  ```
- [x] Build:
  ```bash
  tools/idf53.sh -B build-c3-root build
  ```
- [x] Board smoke:
  ```bash
  tools/idf53.sh -B build-c3-root -p /dev/ttyACM0 flash
  python3 tools/bin_term_smoke.py --port /dev/ttyACM0 --timeout 25
  python3 tools/nano_editor_smoke.py --port /dev/ttyACM0 --timeout 25
  python3 tools/basic_editor_smoke.py --port /dev/ttyACM0 --timeout 25
  ```
- [x] Pass criteria:
  - Build succeeds.
  - Board smoke confirms `/bin/term` escape output.
  - Board smoke confirms BASIC `TERM` bridge.
  - No shell command surface regression.

### T10-T8 - Completion docs and handoff

- [x] Update:
  - `README.md`
  - `SESSION_STATUS.md`
  - `.codex/PROJECT_SKILL.md`
  - `source/bin` docs if present
- [x] Record board evidence.
- [x] Mark Sprint 010 complete only after board smoke passes.
- [x] Set the next candidate milestone back to Sprint 009 ASM nano mode unless
  the user chooses raw-key terminal input first.

## Execution Order

1. T10-T1
2. T10-T2
3. T10-T3
4. T10-T4
5. T10-T5
6. T10-T6
7. T10-T7
8. T10-T8

Untitled editor launch behavior is tracked separately in
`docs/SPRINT_011_UNTITLED_NANO_BASIC_TASK_LIST.md` and should not be mixed into
the BASIC `TERM` bridge implementation unless explicitly selected.

Keep Going.
