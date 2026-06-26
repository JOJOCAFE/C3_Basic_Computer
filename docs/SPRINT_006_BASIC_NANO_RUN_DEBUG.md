# Sprint 006 Planning: BASIC Nano Plugin, Save/Load, Run, and Debug

Status: Task plan for review.

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
  - Current implementation is numbered-line oriented.
  - Current runtime limits are `BASIC_MAX_LINES=64` and `BASIC_LINE_TEXT=128`.
  - Sprint 006 must remove that small stored-line ceiling for editor-run paths
    so BASIC can use the full 64 KiB nano buffer.

## User-Facing Commands

Shell entry points:

```text
BASIC /basic/hello.bas
BASIC /data/scratch.txt
/bin/nano /basic/hello.bas --mode basic
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

`.bas` is the preferred extension. `.txt` remains allowed as plain text in nano;
`:run` and `:debug` may run a `.txt` buffer only if the user launched it in
BASIC mode and the content validates as BASIC.

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

- [ ] Update `docs/04_Shell_Reference.md`.
- [ ] Update `README.md`.
- [ ] Update `source/bin/README.md`.
- [ ] Record `BASIC <path>`, `:run`, and `:debug` as BASIC editor features.
- [ ] Record that `:debug` is BASIC step-run.
- [ ] Record that BASIC editor-run source uses the full 64 KiB nano buffer.
- [ ] Record the safe BASIC shell/hardware bridge.
- [ ] Pass criteria:
  - Docs distinguish `EDIT` plain text mode from `BASIC` editor mode.
  - Docs state that BASIC runtime is invoked from nano, not from boot-shell
    immediate mode.
  - Docs state which shell/hardware calls are allowed and which are blocked.

### B6-T2 - Add BASIC mode path policy

- [ ] File: `source/editor/editor_service.c`
- [ ] Accept `.bas` files under `/basic`.
- [ ] Keep `.txt` files under `/data` valid for normal `EDIT`.
- [ ] Decide whether `BASIC /data/name.txt` is allowed as BASIC mode for scratch
  testing.
- [ ] Pass criteria:
  - `BASIC /basic/hello.bas` opens nano.
  - `EDIT /basic/hello.bas` either opens read/write text safely or returns a
    clear mode/path message.
  - Path traversal with `..` is still rejected.

### B6-T3 - Register BASIC shell alias as editor launcher

- [ ] File: `source/bin/bin_registry.c`
- [ ] File: `source/bin/bin_nano.c`
- [ ] Register `BASIC` as an alias/front end to `/bin/nano` with
  `EDITOR_MODE_BASIC`.
- [ ] Do not add a separate BASIC runtime shell command in this task.
- [ ] Pass criteria:
  - `BASIC /basic/hello.bas` enters nano BASIC mode.
  - `HELP` may list `BASIC` only as an editor launcher if the command is
    implemented and docs match.

### B6-T4 - Add BASIC plugin validation

- [ ] File: `source/editor/editor_service.c` or new editor plugin file.
- [ ] Validate on `:w`, `:wq`, `:run`, and `:debug`.
- [ ] Numbered-line first slice:
  - blank lines allowed
  - `REM` and `'` comment lines allowed only after a valid line number for the
    first runtime-backed slice
  - decimal line number must be followed by whitespace or end-of-line
  - malformed numeric prefixes such as `123ABC` rejected
  - total source must fit the 64 KiB editor buffer
- [ ] Pass criteria:
  - Valid numbered BASIC saves.
  - `123ABC` is rejected before save/run.
  - The editor buffer remains intact after validation errors.

### B6-T5 - Add 64 KiB BASIC buffer-to-runtime loader

- [ ] File: `main/basic.c` / `main/basic.h` or new `source/basic` service.
- [ ] Add a helper that loads a BASIC program from an in-memory text buffer up
  to the nano editor capacity.
- [ ] Build a runtime line index instead of copying each source line into the
  old `BASIC_LINE_TEXT=128` slots.
- [ ] Keep source text owned by the editor/runtime context for the duration of
  run/debug.
- [ ] Preserve current `basic_load_file()` behavior.
- [ ] Return structured validation errors:
  - source line number
  - token/line text excerpt
  - reason
- [ ] Pass criteria:
  - Nano does not need to write a temp file just to validate.
  - `:run` and `:debug` can validate current unsaved edits after saving policy
    is decided.
  - A BASIC file larger than the old 64-line/127-byte model but within 64 KiB
    loads or reports a specific parser/runtime limitation.

### B6-T6 - Implement `:run`

- [ ] File: `source/editor/editor_service.c` and BASIC service/helper.
- [ ] `:run` saves the current buffer first.
- [ ] Load the saved/current buffer into `basic_program_t`.
- [ ] Run with editor I/O:
  - BASIC output prints inside nano session.
  - After program ends, nano returns to `edit>` or `edit*`.
- [ ] Pass criteria:
  - Program:
    ```basic
    10 PRINT "HELLO"
    20 END
    ```
    prints `HELLO`.
  - Syntax/load failure prints a short line-anchored error.
  - Existing file remains open for editing after run.

### B6-T7 - Implement BASIC step-run `:debug`

- [ ] `:debug` executes one BASIC statement per user action.
- [ ] First slice may use simple line prompts instead of full breakpoints.
- [ ] Required debug controls:
  ```text
  Enter or s  step next statement
  c           continue until END or error
  q           quit debug and return to nano
  p           print current BASIC variables/state summary
  l           list current source line/context
  ```
- [ ] Each step prints:
  - current BASIC line number or source index
  - statement text
  - output produced by that statement
  - next line target when known
- [ ] Pass criteria:
  - Valid program can step through at least `PRINT`, `LET`, `GOTO`, `FOR/NEXT`,
    and `END` using the existing runtime semantics.
  - Invalid program reports the same error style as `:run`.
  - `q` exits debug and returns to the nano editor prompt.
  - Debug never executes native ASM or CPU monitor commands.

### B6-T8 - Add BASIC shell and hardware bridge

- [ ] Add explicit BASIC syntax for shell/hardware calls. Candidate syntax:
  ```basic
  100 SHELL "LS /data"
  110 SHELL "CAT /data/note.txt"
  120 HARDWARE "gpio read -p 8"
  ```
- [ ] `SHELL` may call safe workspace shell commands only:
  - `PWD`
  - `LS`
  - `CAT`
  - non-destructive query commands
- [ ] `SHELL` must block protected/destructive/system commands unless a later
  permission model exists:
  - `RENEW`
  - `RM`
  - `RM -R`
  - `WRITE`
  - `CP`
  - `MV`
  - native execution
- [ ] `HARDWARE` should call `source/hardware` typed APIs directly when possible.
  If the first slice routes through `/bin/hardware`, it must use a whitelist and
  remain visibly marked as a temporary adapter.
- [ ] Pass criteria:
  - BASIC can read a directory listing or file content safely.
  - BASIC can call GPIO read through the hardware path.
  - BASIC cannot run `RENEW`, delete files, or invoke native execution.

### B6-T9 - Add board smoke for BASIC nano mode

- [ ] Add `tools/basic_editor_smoke.py`.
- [ ] Cover:
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
  - `.txt` scratch mode if accepted by B6-T2
- [ ] Pass criteria:
  - Smoke exits with nonzero status on any missing output or prompt loss.
  - Existing `nano_editor_smoke.py` still passes.

### B6-T10 - Regression gate

- [ ] Build root firmware.
- [ ] Flash board.
- [ ] Run:
  ```bash
  tools/idf53.sh -C . -B build-c3-root build
  python3 tools/workspace_shell_smoke.py --port /dev/ttyACM0
  python3 tools/nano_editor_smoke.py --port /dev/ttyACM0 --timeout 25
  python3 tools/basic_editor_smoke.py --port /dev/ttyACM0
  python3 tools/bin_pipe_smoke.py --port /dev/ttyACM0
  python3 tools/bin_hardware_gpio_smoke.py --port /dev/ttyACM0 --pin 8 --seconds 10
  ```
- [ ] Pass criteria:
  - Plain nano mode still supports `.txt`.
  - BASIC mode supports `.bas`.
  - No BASIC runtime appears as raw boot-shell immediate mode.
  - Shell recovery and `RENEW` behavior stay independent of `/basic` files.

## Open Decisions Before Coding

1. Should `BASIC /data/name.txt` allow `:run`, or should `:run` require `.bas`
   under `/basic`?
2. Should `:run` always save first, or ask when the buffer is dirty?
3. Should firmware `HELP` list `BASIC` once it is implemented as an editor
   launcher, or should it remain discoverable only through `/bin list` and docs?
4. Should first `HARDWARE` syntax be string-command based for speed, or typed
   BASIC statements/functions over `source/hardware`?

## Non-Goals

- No modern unnumbered BASIC execution unless the runtime parser is explicitly
  extended.
- No ASM capture or execution in this sprint.
- No CPU/native stepping, breakpoints, watch variables, or monitor integration yet.
- No loadable workspace executables.
