# C3 BASIC COMPUTER

# 04 Shell Reference

Status: Implemented, Sprint 002 complete

---

## Purpose

The Shell is the default environment of C3 BASIC COMPUTER.

It provides access to the computer, the Workspace, programs and system services.

Every session begins here.

---

## Boot

```text
C3 BASIC COMPUTER

READY.

>
```

The computer is immediately ready.

---

## Prompt

The shell prompt is

```text
>
```

The current working directory is intentionally not displayed.

Use

```text
PWD
```

to display it.

---

## Command Style

The Shell is a small micro Linux workspace shell.

Rules

* document shell commands in uppercase
* accept command keywords case-insensitively in firmware
* space-separated arguments
* Linux-style paths
* keep file commands inside `/workspace`

Examples

```text
LS

CD basic

CAT hello.bas

WRITE temp/note.txt hello

CP temp/note.txt temp/copy.txt
```

---

## Workspace

The Shell works inside the Workspace.

Default structure

```text
/

/basic
/asm
/bin
/config
/data
/temp
```

Directory listings print directories with a leading `/` and files without one.

---

## Command Groups

Current shell behavior is organized into three groups.

```text
Core

Workspace

Deferred
```

---

## Core Commands

```text
HELP

PWD

LS

CD

MKDIR

RMDIR

CAT

WRITE

RM

CP

MV
```

---

## Deferred Workspace Commands

BASIC is not exposed directly from the boot shell.

Planned workspace editor commands:

```text
EDIT

BASIC

ASM
```

Examples

```text
> WRITE basic/hello.bas 10 PRINT "HELLO"

> CAT basic/hello.bas

> CP basic/hello.bas basic/copy.bas

> MV basic/copy.bas basic/renamed.bas

> RMDIR temp/empty
```

`EDIT <path>` is the shell command for the text editor. Later,
`BASIC <path>` and `ASM <path>` should call the same nano editor service with
the BASIC or ASM plugin selected. Native ASM execution remains blocked until a
later guarded runtime sprint.

---

## System Commands

Only one system command is exposed by the current boot shell:

```text
RENEW
```

`RENEW` asks twice, formats only `workspace_fs`, recreates the default
workspace layout, and returns to the prompt. Broader system information
commands such as `VERSION`, `MEMORY`, `DATE`, `TIME`, and `DIAGNOSTICS` are
deferred and must not appear in firmware `HELP` until implemented.

---

## `/bin` Services

The root firmware can register external `/bin` services above the shell. These
services are not shell built-ins and do not appear in `HELP`.

Current `/bin/hardware` service:

```text
/bin list
/bin/hardware gpio in -p <gpio> [--pull none|up|down|updown]
/bin/hardware gpio out -p <gpio> [-v 0|1] [--open-drain]
/bin/hardware gpio read -p <gpio>
/bin/hardware gpio write -p <gpio> -v 0|1
/bin/hardware gpio toggle -p <gpio>
/bin/hardware adc read -p <gpio>
/bin/hardware i2c config -sda <gpio> -scl <gpio> [-f <hz>] [--pullups]
/bin/hardware i2c probe -a <addr>
/bin/hardware i2c scan
/bin/hardware spi config -mosi <gpio> -miso <gpio> -sclk <gpio> [-cs <gpio>] [-f <hz>] [-m <mode>]
/bin/hardware spi xfer -tx <hexbytes>
HARDWARE gpio read -p <gpio>
```

The hardware implementation lives in `source/hardware` and the text command
adapter lives in `source/bin`. The standalone `source/shell` project does not
link either package.

Current `/bin/nano` editor service:

```text
/bin/nano <path>
EDIT <path>
BASIC <path>
```

`/bin/nano` is a small nano-style text editor service. `EDIT <path>` is the
plain text shell launcher and edits `/data/*.txt` files only. `BASIC <path>` is
the BASIC editor launcher and edits `/basic/*.bas` files only. BASIC mode
validates numbered BASIC lines before save. BASIC runtime behavior is invoked
from nano commands, not from boot-shell immediate mode.

BASIC runtime editor commands:

```text
:run    Save, validate, and run the open BASIC program
:debug  Step-run the open BASIC program one statement at a time
```

`:run` and `:debug` are implemented for numbered BASIC programs and use a
buffer-backed loader that can consume the full 64 KiB nano text buffer.
`:debug` is BASIC statement stepping, not CPU or native monitor stepping.

The first BASIC runtime is intentionally tiny: numbered lines, integer
expressions, single-letter variables, `REM`, `PRINT`, `LET`/assignment,
`INPUT`, `IF THEN [ELSE]`, `GOTO`, `GOSUB`/`RETURN`, `FOR`/`NEXT`, `END`/`STOP`,
and `ABS`, `INT`, `RND`. Extended graphics, sound, network, arrays, strings,
and modern unnumbered BASIC are deferred.

BASIC shell and hardware access must go through an explicit safe bridge. The
current bridge allows `SHELL "PWD"`, `SHELL "CAT <file>"`, `HARDWARE "gpio read
-p <pin>"`, and `HARDWARE "adc read -p <pin>"`. Directory listing from BASIC is
deferred until it has a stack-safe adapter. The bridge blocks destructive or
protected commands such as `RENEW`, `RM`, `RM -R`, `WRITE`, `CP`, `MV`, and
native execution. `ASM <path>` remains a planned shell front end to nano ASM
mode.

Current editor limits:

- Text buffer: 64 KiB per open file, including line separators.
- Input line buffer: 64 KiB, bounded by the same editor buffer capacity.
- If editor allocation fails, it prints `Out of memory` and returns to the
  shell instead of entering a partial editor state.
- Files larger than the editor buffer are rejected with `File too large`.
- If appending text would exceed the editor buffer, it prints `Buffer full` and
  returns to the shell.

The first editor command set should stay small:

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

Detailed planning lives in
[`docs/SPRINT_004_NANO_EDITOR_SERVICE.md`](SPRINT_004_NANO_EDITOR_SERVICE.md).
The `/bin` service registry and future pipe ABI are tracked in
[`docs/SPRINT_005_BIN_SERVICE_ABI_AND_PIPES.md`](SPRINT_005_BIN_SERVICE_ABI_AND_PIPES.md).
The first RAM-backed pipe supports:

```text
CAT /data/input.txt | WRITE /data/output.txt
```

---

## Monitor Commands

```text
reg

mem

dump

disasm

step

break
```

Monitor Commands provide visibility into the computer.

They are intended for exploration, debugging and learning. They are deferred
until after the shell-first and ASM-boundary work.

Detailed behavior is documented in the Monitor documentation.

---

## Help

`HELP` prints the implemented firmware command list only:

```text
HELP PWD LS CD MKDIR RMDIR CAT WRITE RM CP MV RENEW
```

Per-command help is deferred. BASIC, ASM, system information, graphics, sound,
hardware services, and monitor commands must stay out of `HELP` until their
runtime behavior is implemented and tested as shell commands. Current hardware
access remains a `/bin` service.

---

## System Messages

### READY.

Displayed whenever the computer is ready for the next command.

---

### ERROR.

Errors should explain

* what happened
* where it happened (when applicable)

Example

```text
ERROR.

File not found.

hello.bas
```

Numeric error codes should be avoided whenever practical.

---

### WARNING.

Reserved for operations that may permanently remove user data.

Example

```text
WARNING.

This operation removes
all user programs and data.

Do you understand? (Y/N)
```

---

## Design Rules

The Shell should remain

* Small
* Predictable
* Consistent
* Discoverable

The Shell is the front door of C3 BASIC COMPUTER.

Everything should be reachable from the Shell.

The Shell should never become more complicated than necessary.

---

## Compatibility

Shell behavior should remain consistent across the C-Series BASIC COMPUTER family whenever practical.

Reference platforms include

* C3 BASIC COMPUTER
* C5 BASIC COMPUTER
* C6 BASIC COMPUTER
* P4 BASIC COMPUTER

---

Keep Going.
