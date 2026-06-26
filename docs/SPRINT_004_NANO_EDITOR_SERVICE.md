# Sprint 004 Planning: Nano-Style Editor Service

Status: First `.txt` editor slice implemented and board-tested; BASIC/ASM plugins pending.

Goal: add a tiny text editor service named `nano`. The first implementation
edits `.txt` files only. BASIC and ASM support come later as removable editor
plugins that are launched through the same nano service.

Current `.txt` editor capacity:

- 64 KiB text buffer per open file.
- Input line buffer is allocated to the same 64 KiB bound.
- Allocation failure prints `Out of memory` and returns a shell error instead
  of entering a partial editor state.
- Existing files larger than the editor buffer are rejected with
  `File too large`.
- Appending beyond the editor buffer prints `Buffer full` and returns to the
  shell.

This sprint uses GNU nano as a behavior reference only. Do not import nano code,
license text, terminal assumptions, runtime configuration, syntax engines, shell
execution features, or full shortcut surface into C3.

Reference checked:

```text
https://git.savannah.gnu.org/git/nano.git
```

The Savannah server redirects that repository to:

```text
https://https.git.savannah.gnu.org/git/nano.git/
```

The reference commit observed during planning was:

```text
e1d954c739bd8a5d19fc1861c224fb7d03606e8d
```

## Design Decision

The editor is a `/bin` service with a shell command front end.

Canonical service command:

```text
/bin/nano <path>
```

Public shell command:

```text
EDIT <path>
```

Future language commands:

```text
BASIC <path>
ASM <path>
```

`BASIC <path>` launches nano with the BASIC editor plugin. `ASM <path>`
launches nano with the ASM editor plugin. These commands are shell front ends to
the editor service, not BASIC or assembler runtimes.

The shell starts the service and returns to the prompt when the service exits.
The shell must not know BASIC syntax, ASM syntax, editor buffer internals, or
plugin details.

## Placement

Proposed source layout:

```text
source/bin/bin_nano.c
source/editor/
source/editor/editor_service.h
source/editor/editor_service.c
source/editor/editor_buffer.c
source/editor/editor_input.c
source/editor/editor_screen.c
source/editor/plugins/
source/editor/plugins/basic_editor_plugin.c
source/editor/plugins/asm_editor_plugin.c
```

`source/editor` owns the editor core. `source/bin/bin_nano.c` is the `/bin`
service adapter that registers `/bin/nano` and calls the editor core.
`source/shell` should only dispatch the command.

## Lean Nano Command Set

The C3 editor should borrow nano's simple mental model: type text, move around,
save, quit, search, cut one line, and paste.

Required first command set:

```text
Text line  Append text
:w         Save
:q         Quit if clean
:q!        Quit without saving
:wq        Save and quit
:p         Print buffer
:clear     Clear buffer
:help      Help
```

This first implementation is line-oriented so it can run over the current shell
input service. Full cursor editing waits for a later raw-key input boundary.

Optional later command set:

```text
Ctrl-S  Save
Ctrl-Q  Quit
Ctrl-G  Help
Ctrl-F  Find
Ctrl-K  Cut current line
Ctrl-U  Paste
Arrow keys  Move cursor
Backspace  Delete before cursor
Enter  Insert newline
Printable keys  Insert text
Ctrl-O  Save As
Ctrl-A  Mark
Ctrl-C  Copy marked text
Ctrl-X  Cut marked text
Ctrl-Z  Undo
Ctrl-Y  Redo
PageUp/PageDown  Scroll
Home/End  Line boundary
```

Do not implement the optional set until the required set is stable on both PC
terminal input and the future BLE keyboard input path.

## Screen Contract

The editor should be usable on a simple serial terminal.

Minimum screen regions:

```text
title line:   EDIT  /data/note.txt  MODIFIED
body:         editable text
status line:  SAVED / ERROR text
help line:    :w Save  :q Quit  :wq Save+Quit  :help Help
```

Rules:

- Plain text remains the storage format.
- No mouse support.
- No shell escape.
- No external spell checker.
- No runtime `nanorc` equivalent.
- No syntax color in the first implementation.
- No multi-buffer editing in the first implementation.
- No native ASM execution from inside the editor.

## Plugin Boundary

BASIC and ASM are editor plugins/services, not editor core logic. They are not
part of the first `.txt` implementation.

Editor core provides:

```text
open file
edit buffer
save file
dirty flag
cursor movement
line cut/paste
status messages
plugin callback registration
```

Plugin callbacks may provide:

```text
file extension match
pre-save validation
post-save sidecar update
line classification
mode name for title/status
future syntax highlighting
future run/build hooks
```

Plugins must not own:

```text
keyboard driver
screen driver
filesystem path resolver
shell command dispatch
protected recovery behavior
```

## BASIC Plugin

Later BASIC plugin scope:

- Match `.bas` files under `/basic`.
- Validate that numbered program lines start with a decimal line number followed
  by whitespace or end of line.
- Allow blank lines and comments as plain text.
- Reject malformed numeric prefixes such as `123ABC`.
- Do not run BASIC from the editor.
- Do not expose BASIC in boot-shell `HELP`.

Future BASIC runtime scope:

- Optional format command.
- Optional `RUN` handoff to a BASIC runtime service.
- Optional lint messages for unknown statements.

## ASM Plugin

Later ASM plugin scope:

- Match `.asm` files under `/asm`.
- Treat ASM as plain text plus validation.
- Validate line length, label shape, and unsupported binary/control characters.
- Allow comments.
- Do not assemble or execute.
- Do not expose native execution in boot-shell `HELP`.

Future ASM runtime scope:

- Optional assembler validation service.
- Optional object sidecar generation.
- Optional runtime handoff after the guarded native execution sprint exists.

## Storage Rules

The editor works only inside `/workspace`.

First target path:

```text
/data/*.txt
```

Later plugin paths:

```text
/basic/*.bas
/asm/*.asm
/config/*.cfg
```

Rules:

- Reject path traversal with `..`.
- Reject writes outside `/workspace`.
- Save through a temporary file plus rename when the filesystem adapter supports
  it.
- On save failure, keep the editor buffer alive and display a short error.
- `RENEW` remains protected and outside editor control.

## Service Lifecycle

The shell starts `/bin/nano` with argv-like text arguments. The `EDIT` command
maps directly to `/bin/nano`. Later, `BASIC` and `ASM` map to `/bin/nano` with a
language plugin selected.

Lifecycle:

```text
shell parses command
bin registry finds nano service
nano opens path through storage service
nano runs modal editor loop
nano saves or discards changes
nano returns status code
shell prints prompt again
```

Exit status:

```text
0  clean exit
1  user canceled with unsaved changes
2  invalid path or open failure
3  save failure
```

## Task List

### N4-T0 - Review and freeze editor boundary

- [x] Review this document.
- [x] Use `EDIT <path>` as the shell command for the text editor.
- [x] Keep canonical service name `/bin/nano`.
- [x] Make future `BASIC <path>` and `ASM <path>` shell commands call nano with
  language plugins.
- [x] First implementation supports `.txt` only before `.bas` and `.asm`
  plugins.
- [x] Pass criteria:
  - No code changes required.
  - Project docs agree that `/bin/nano` is a service, not a shell built-in.

### N4-T1 - Add editor service interface

- [x] Add `source/editor/editor_service.h`.
- [x] Define open/run/exit status API.
- [x] Keep storage and input behind the existing shell/input/storage service
  boundaries.
- [x] Pass criteria:
  - Header compiles standalone.
  - Shell does not include editor internals.

### N4-T2 - Add minimal text buffer

- [x] Add line-oriented text buffer for `.txt`.
- [x] Support appending entered lines and clearing the buffer.
- [x] Track dirty state.
- [x] Pass criteria:
  - Root firmware builds with editor buffer code.
  - Full cursor editing, cut line, and paste line remain later raw-key work.

### N4-T3 - Add terminal editor loop

- [x] Route input through the existing input service boundary.
- [x] Implement required line command set only.
- [x] Keep rendering simple and serial-terminal safe.
- [x] Pass criteria:
  - Open, edit, save, quit is implemented for USB Serial/JTAG.
  - Root firmware build passes.

### N4-T4 - Register `/bin/nano`

- [x] Add `source/bin/bin_nano.c`.
- [x] Register `/bin/nano <path>`.
- [x] Register shell command `EDIT <path>` as a front end to `/bin/nano`.
- [x] Return clean status to shell.
- [x] Pass criteria:
  - `/bin/nano /data/test.txt` opens from shell.
  - `EDIT /data/test.txt` opens from shell.
  - Exiting returns to shell prompt.

### N4-T5 - Add BASIC editor plugin

- [x] Add `.bas` mode detection.
- [x] Register shell command `BASIC <path>` as a front end to `/bin/nano` with
  BASIC plugin mode.
- [x] Add pre-save numeric-line validation.
- [x] Keep BASIC runtime separate.
- [x] Pass criteria:
  - `123ABC` is rejected.
  - Valid numbered BASIC text saves.
  - No BASIC run command appears in boot-shell `HELP`.

### N4-T6 - Add ASM editor plugin

- [ ] Add `.asm` mode detection.
- [ ] Register shell command `ASM <path>` as a front end to `/bin/nano` with ASM
  plugin mode.
- [ ] Add safe text validation.
- [ ] Keep assembler/runtime separate.
- [ ] Pass criteria:
  - Malformed binary/control input is rejected.
  - Valid ASM text saves.
  - No native execution is enabled.

### N4-T7 - Board smoke

- [x] Flash current firmware.
- [x] Run existing shell smokes.
- [x] Add nano smoke for open/edit/save/quit.
- [ ] Add BASIC and ASM plugin validation smokes.
- [x] Pass criteria:
  - Existing shell smokes pass.
  - `/bin/nano` smoke passes.
  - `RENEW` still rebuilds workspace without depending on editor files.

Board verification:

- 2026-06-27: `tools/idf53.sh -C . -B build-c3-root build` passed.
- 2026-06-27: `tools/idf53.sh -C . -B build-c3-root -p /dev/ttyACM0 flash` passed.
- 2026-06-27: `python3 tools/workspace_shell_smoke.py --port /dev/ttyACM0` passed.
- 2026-06-27: `python3 tools/no_basic_shell_smoke.py --port /dev/ttyACM0` passed.
- 2026-06-27: `python3 tools/adversarial_shell_smoke.py --port /dev/ttyACM0` passed.
- 2026-06-27: `python3 tools/renew_full_smoke.py --port /dev/ttyACM0` passed.
- 2026-06-27: `python3 tools/nano_editor_smoke.py --port /dev/ttyACM0` passed
  for open, append, `:help`, `:p`, `:w`, clean `:q`, dirty `:q` refusal,
  `:q!`, `:clear`, `:wq`, direct `/bin/nano`, near-64 KiB file content,
  over-64 KiB `Buffer full`, long lines, and binary-ish hostile input.
- Fix recorded during board verification: `editor_run()` now keeps the editor buffer on heap instead of the main task stack.

## Non-Goals

- No full GNU nano port.
- No `nanorc`.
- No syntax color in the first sprint.
- No shell escape.
- No spell check.
- No native ASM execution.
- No BASIC runtime launch from the editor until the BASIC runtime service exists.
- No dependency from protected recovery shell to workspace `/bin` files.
