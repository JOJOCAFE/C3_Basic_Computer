# Sprint 006 Planning: BASIC Nano Plugin, Save/Load, Run, and Debug

Status: Tasks B6-T1 through B6-T10 implemented and board-checked for the tiny
numbered BASIC slice.

Goal: make BASIC programs editable from the nano service, save/load `.bas`
files as UTF-8 plain text, and allow a maker to run or debug the current BASIC
file without leaving nano.

This sprint changes the earlier placeholder rule that BASIC editor plugins do
not run programs. The new rule is narrower:

```text
BASIC <path> starts nano in BASIC mode.
:run and :debug are editor commands that hand the current buffer to a BASIC
runtime service.
:debug means step-run: execute one BASIC statement at a time with visible state.
The boot shell still does not become an immediate-mode BASIC prompt.
```

Native ASM execution remains blocked. `:run` and `:debug` apply only to BASIC
source handled by `main/basic.c` or a later split BASIC service.

## Source Specs Checked

- `docs/06_BASIC_Language.md`
  - BASIC programs are UTF-8 plain text.
  - `.bas` is the program file extension.
  - Classic numbered and modern unnumbered formats are both design goals.
- `docs/08_Workspace.md`
  - Every supported file uses the same editor.
  - `run` executes the current `.bas` program.
  - Workspace stays language-independent; file type selects behavior.
- `docs/SPRINT_004_NANO_EDITOR_SERVICE.md`
  - Nano owns editor lifecycle, storage path resolution, and plugin callbacks.
  - BASIC plugin may validate numbered line format.
  - Shell must not know BASIC syntax.
- Current `main/basic.h` / `main/basic.c`
  - Existing runtime API: `basic_load_file`, `basic_save_file`, `basic_run`.
- `https://github.com/slviajero/tinybasic`
  - Cloned and checked locally on 2026-06-27.
  - Basic2 POSIX builds with `gcc basic.c runtime.c -lm`.
  - The upstream POSIX binary can run a sample program, but its stock
    `main()`/`loop()` model remains an interactive prompt after stdin ends.
  - Directly compiling `Basic2/Posix/runtime.c` into ESP-IDF is not the right
    port path because it assumes POSIX terminal, signal, file, and serial APIs.
  - The practical port boundary is a C3 BASIC service API: nano passes the
    64 KiB text buffer to a runtime helper, while an upstream TinyBasic adapter
    can later sit behind that helper.
- Current `main/basic.h` / `main/basic.c`
  - Runtime API: `basic_load_buffer`, `basic_load_file`, `basic_save_file`,
    `basic_run`, `basic_debug`.
  - Current implementation is numbered-line oriented.
  - The old fixed stored-line model has been replaced for editor-run paths.
  - Tiny core retained from the reference direction: numbered lines, integer
    expressions, single-letter variables, `REM`, `PRINT`, `LET`/assignment,
    `INPUT`, `IF THEN [ELSE]`, `GOTO`, `GOSUB`/`RETURN`, `FOR`/`NEXT`,
    `END`/`STOP`, and `ABS`, `INT`, `RND`.
  - Extended graphics, sound, network, arrays, strings, and modern unnumbered
    BASIC remain deferred.

## User-Facing Commands

Shell entry points:

```text
BASIC /basic/hello.bas
```

Nano BASIC-mode commands:

```text
Text line  Append text
:w         Save
:q         Quit if clean
:q!        Quit without saving
:wq        Save and quit
:p         Print buffer
:clear     Clear buffer
:help      Help
:run       Save, load current buffer as BASIC, run it
:debug     Save, load current buffer as BASIC, step-run one statement at a time
```

`.bas` under `/basic` is required for BASIC mode. `.txt` remains plain text in
nano through `EDIT /data/name.txt`.

## Boundary Rules

- `BASIC <path>` is an editor launcher, not immediate BASIC mode.
- `EDIT <path>` remains plain `.txt` mode.
- `/bin/nano` remains the canonical editor service.
- BASIC run/debug logic must call a typed BASIC service/helper, not shell text
  commands.
- BASIC source is saved as plain UTF-8 text.
- BASIC source should use the full 64 KiB nano buffer. Runtime parsing may build
  an execution index from that buffer, but must not truncate the source back to
  the old 64-line / 127-byte line storage model.
- First implementation should support numbered BASIC lines for execution. Modern
  unnumbered BASIC remains design-goal text until the parser/runtime is extended.
- Modern unnumbered BASIC format remains a later parser/runtime task unless a
  small adapter can be implemented without weakening validation.
- Debug must not require native monitor features. It is BASIC step execution,
  not CPU single-step.
- BASIC may call controlled shell and hardware commands through a typed bridge.
  It must not call protected recovery commands or arbitrary native execution.
- Current bridge scope is intentionally small:
  - `SHELL "PWD"`
  - `SHELL "CAT <file>"`
  - `HARDWARE "gpio read -p <pin>"`
  - `HARDWARE "adc read -p <pin>"`
- Directory listing from BASIC is deferred until it has a stack-safe adapter.

## Runtime Constraints To Replace Or Surface

The old runtime limits are too small for the nano workflow:

```text
Old maximum stored BASIC lines: 64
Old maximum stored BASIC line text: 127 bytes plus terminator
New BASIC source budget for nano-run path: 64 KiB text buffer
```

Sprint 006 should replace the old fixed `basic_program_t` storage for editor-run
paths with a buffer-backed parser/index. If a lower-level runtime path still has
an old limit, the editor must report it explicitly and keep the source buffer
intact.

Validation rules that remain:

```text
Numbered line required for first run/debug slice
Malformed numeric prefix such as 123ABC is invalid
Line text must fit within the 64 KiB source buffer
```

## Task List

### B6-T1 - Freeze BASIC editor command contract

- [x] Update `docs/04_Shell_Reference.md`.
- [x] Update `README.md`.
- [x] Update `source/bin/README.md`.
- [x] Record `BASIC <path>`, `:run`, and `:debug` as BASIC editor features.
- [x] Record that `:debug` is BASIC step-run.
- [x] Record that BASIC editor-run source uses the full 64 KiB nano buffer.
- [x] Record the safe BASIC shell/hardware bridge.
- [x] Pass criteria:
  - Docs distinguish `EDIT` plain text mode from `BASIC` editor mode.
  - Docs state that BASIC runtime is invoked from nano, not from boot-shell
    immediate mode.
  - Docs state which shell/hardware calls are allowed and which are blocked.

### B6-T2 - Add BASIC mode path policy

- [x] File: `source/editor/editor_service.c`
- [x] Accept `.bas` files under `/basic`.
- [x] Keep `.txt` files under `/data` valid for normal `EDIT`.
- [x] Decide whether `BASIC /data/name.txt` is allowed as BASIC mode for scratch
  testing: it is rejected; BASIC mode is `/basic/*.bas` only.
- [x] Pass criteria:
  - `BASIC /basic/hello.bas` opens nano.
  - `EDIT /basic/hello.bas` either opens read/write text safely or returns a
    clear mode/path message.
  - Path traversal with `..` is still rejected.

### B6-T3 - Register BASIC shell alias as editor launcher

- [x] File: `source/bin/bin_registry.c`
- [x] File: `source/bin/bin_nano.c`
- [x] Register `BASIC` as a front end to the nano editor service with
  `EDITOR_MODE_BASIC`.
- [x] Do not add a separate BASIC runtime shell command in this task.
- [x] Pass criteria:
  - `BASIC /basic/hello.bas` enters nano BASIC mode.
  - `HELP` may list `BASIC` only as an editor launcher if the command is
    implemented and docs match.

### B6-T4 - Add BASIC plugin validation

- [x] File: `source/editor/editor_service.c` or new editor plugin file.
- [x] Validate on `:w`, `:wq`, `:run`, and `:debug`.
- [x] Numbered-line first slice:
  - blank lines allowed
  - `REM` and `'` comment lines allowed only after a valid line number for the
    first runtime-backed slice
  - decimal line number must be followed by whitespace or end-of-line
  - malformed numeric prefixes such as `123ABC` rejected
  - total source must fit the 64 KiB editor buffer
- [x] Pass criteria:
  - Valid numbered BASIC saves.
  - `123ABC` is rejected before save; run/debug validation is covered when
    those editor commands exist.
  - The editor buffer remains intact after validation errors.

### B6-T5 - Add 64 KiB BASIC buffer-to-runtime loader

- [x] File: `main/basic.c` / `main/basic.h` or new `source/basic` service.
- [x] Add a helper that loads a BASIC program from an in-memory text buffer up
  to the nano editor capacity.
- [x] Build a runtime line index instead of copying each source line into the
  old `BASIC_LINE_TEXT=128` slots.
- [x] Keep source text owned by the editor/runtime context for the duration of
  run/debug.
- [x] Preserve current `basic_load_file()` behavior.
- [ ] Return structured validation errors:
  - source line number
  - token/line text excerpt
  - reason
- [x] Pass criteria:
  - Nano does not need to write a temp file just to validate.
  - `:run` and `:debug` validate current edits after saving.
  - A BASIC file larger than the old 64-line/127-byte model but within 64 KiB
    loads or reports a specific parser/runtime limitation.

### B6-T6 - Implement `:run`

- [x] File: `source/editor/editor_service.c` and BASIC service/helper.
- [x] `:run` saves the current buffer first.
- [x] Load the saved/current buffer into `basic_program_t`.
- [x] Run with editor I/O:
  - BASIC output prints inside nano session.
  - After program ends, nano returns to `edit>` or `edit*`.
- [x] Pass criteria:
  - Program:
    ```basic
    10 PRINT "HELLO"
    20 END
    ```
    prints `HELLO`.
  - Syntax/load failure prints a short line-anchored error.
  - Existing file remains open for editing after run.

### B6-T7 - Implement BASIC step-run `:debug`

- [x] `:debug` executes one BASIC statement per user action.
- [x] First slice uses simple line prompts instead of full breakpoints.
- [x] Required debug controls:
  ```text
  Enter or s  step next statement
  c           continue until END or error
  q           quit debug and return to nano
  p           print current BASIC variables/state summary
  l           list current source line/context
  ```
- [x] Each step prints:
  - current BASIC line number or source index
  - statement text
  - output produced by that statement
  - next line context through the current debug prompt/list command
- [x] Pass criteria:
  - Valid program can step through `LET`, `PRINT`, assignment, and `END` using
    the existing runtime semantics.
  - Invalid program reports the same error style as `:run`.
  - `q` exits debug and returns to the nano editor prompt.
  - Debug never executes native ASM or CPU monitor commands.

### B6-T8 - Add BASIC shell and hardware bridge

- [x] Add explicit BASIC syntax for shell/hardware calls:
  ```basic
  100 SHELL "PWD"
  110 SHELL "CAT /data/note.txt"
  120 HARDWARE "gpio read -p 8"
  ```
- [x] `SHELL` may call safe workspace shell commands only:
  - `PWD`
  - `CAT`
- [x] `LS` is deferred from BASIC until it has a stack-safe adapter.
- [x] `SHELL` must block protected/destructive/system commands unless a later
  permission model exists:
  - `RENEW`
  - `RM`
  - `RM -R`
  - `WRITE`
  - `CP`
  - `MV`
  - native execution
- [x] `HARDWARE` calls `source/hardware` typed APIs directly for read-only
  `gpio read` and `adc read`.
- [x] Pass criteria:
  - BASIC can call `PWD` safely.
  - BASIC can read file content safely through `CAT`.
  - BASIC can call GPIO read through the hardware path.
  - BASIC cannot run `RENEW`, delete files, or invoke native execution.

### B6-T9 - Add board smoke for BASIC nano mode

- [x] Add `tools/basic_editor_smoke.py`.
- [x] First slice covers BASIC open/save/reopen, path policy, invalid numeric-line
  rejection, `:run`, larger-than-old-runtime program load/run, and HELP
  non-exposure.
- [x] Cover:
  - `BASIC /basic/hello.bas`
  - append valid numbered program
  - `:w`
  - `:run`
  - `:debug`
  - step through at least two BASIC statements
  - BASIC shell call allowed path
  - BASIC hardware GPIO read path
  - blocked BASIC shell destructive command
  - `:wq`
  - reopen same `.bas` file and verify contents load
  - malformed `123ABC` rejected
  - `/data/*.txt` rejected from BASIC mode
- [x] Pass criteria:
  - Smoke exits with nonzero status on any missing output or prompt loss.
  - Existing `nano_editor_smoke.py` still passes.

### B6-T10 - Regression gate

- [x] Build root firmware.
- [x] Flash board.
- [x] Run:
  ```bash
  tools/idf53.sh -C . -B build-c3-root build
  python3 tools/workspace_shell_smoke.py --port /dev/ttyACM0
  python3 tools/nano_editor_smoke.py --port /dev/ttyACM0 --timeout 45
  python3 tools/basic_editor_smoke.py --port /dev/ttyACM0 --timeout 40
  python3 tools/no_basic_shell_smoke.py --port /dev/ttyACM0
  python3 tools/bin_pipe_smoke.py --port /dev/ttyACM0
  python3 tools/bin_hardware_gpio_smoke.py --port /dev/ttyACM0 --pin 8 --seconds 10
  ```
- [x] Pass criteria:
  - Plain nano mode still supports `.txt`.
  - BASIC mode supports `.bas`.
  - No BASIC runtime appears as raw boot-shell immediate mode.
  - Shell recovery and `RENEW` behavior stay independent of `/basic` files.

2026-06-27 board evidence:

- Build passed, app size `0x501d0`, 75% free.
- Flash to `/dev/ttyACM0` passed.
- `tools/basic_editor_smoke.py --port /dev/ttyACM0 --timeout 40` passed.
- `tools/nano_editor_smoke.py --port /dev/ttyACM0 --timeout 45` passed.
- `tools/workspace_shell_smoke.py --port /dev/ttyACM0` passed.
- `tools/no_basic_shell_smoke.py --port /dev/ttyACM0` passed.
- `tools/bin_pipe_smoke.py --port /dev/ttyACM0` passed.
- `tools/bin_hardware_gpio_smoke.py --port /dev/ttyACM0 --pin 8 --seconds 10`
  passed.

## Open Decisions Before Coding

1. Add structured BASIC validation errors with source line number, excerpt, and
   reason instead of the current short error.
2. Decide whether `LS` needs a stack-safe BASIC adapter, or whether `CAT`/typed
   service calls are enough for the tiny core.
3. Decide whether hardware reads should grow into typed BASIC statements after
   the string bridge has more test coverage.

## Non-Goals

- No modern unnumbered BASIC execution unless the runtime parser is explicitly
  extended.
- No ASM capture or execution in this sprint.
- No CPU/native stepping, breakpoints, watch variables, or monitor integration yet.
- No loadable workspace executables.
