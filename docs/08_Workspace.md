# C3 BASIC COMPUTER

# 08 Workspace

Status: Current implementation plus design goals

---

## Purpose

The Workspace is the maker's primary working environment.

It is where ideas become programs.

Every maker spends most of their time here.

---

## Design Goals

The Workspace should be

* Simple
* Fast
* Predictable
* Keyboard-first
* Language-independent

---

## Supported Files

The current workspace is the nano editor service launched from the shell.

Current entry points

```text id="wk-current-entry"
EDIT /data/name.txt
BASIC /basic/name.bas
/bin/nano /data/name.txt
```

Design goal: the Workspace should open all supported text files through one
consistent editor model.

Examples

```text id="t2i06x"
.bas

.asm   future

.txt

.cfg   future
```

Current firmware uses nano for `.txt` and `.bas`. Assembly and config editor
flows remain future work.

---

## Plain Text

All editable text files are UTF-8 plain text.

The Workspace never stores proprietary editor formats.

---

## Primary Actions

```text id="oh2b8c"
open file from shell command

save

save as

exit

run BASIC

debug BASIC

help
```

The implemented commands are nano colon commands. From inside nano:

```text id="wk-current-actions"
:w
:w /path/name
:q
:run
:debug
:help
```

Find, go to line, and assemble are design goals, not current firmware
commands.

---

## Running Programs

When the current file is

```text id="4u18g4"
.bas
```

the nano command

```text id="pmrbp9"
:run
```

executes the current program.

The debug command

```text id="wk-debug"
:debug
```

starts step mode. The debug prompt accepts `s`, `n`, `c`, `p`, and `q`.

---

## Assembling Programs

When the current file is

```text id="fjqt2u"
.asm
```

assembly from the workspace is a future feature. The planned command is

```text id="rprn1g"
:asm
```

but native ASM editing/assembly/run has not been implemented yet.

---

## HELP Integration

HELP is available directly from the Workspace.

Future examples

```text id="q29k08"
help line

help sound

help asm
```

The maker should eventually never leave the Workspace to look for
documentation. Current nano help is available with `:help`.

---

## Keyboard

The Workspace is designed for keyboard operation.

Mouse or touch support may be added in future versions.

Keyboard remains the primary interface.

---

## Editing

Implemented editing capabilities

```text id="r4e5ep"
Insert

Delete

cursor movement

line editing

save

save as

quit
```

Copy, paste, undo, redo, find, and replace are future goals.

---

## Navigation

Implemented navigation

```text id="dgkvlg"
Beginning of File

End of File

Page Up

Page Down
```

Go to line is a future goal.

---

## Language Independence

The Workspace edits text and can attach language actions to file modes.

Current BASIC mode is enabled through `BASIC /basic/name.bas` and nano
`:run`/`:debug`.

Assembly mode is not implemented yet.

The file determines how it is used.

---

## Discoverability

The Workspace should encourage exploration.

Examples

```text id="k2gnyb"
:help

:run

:debug
```

are available without leaving the current BASIC file. Future workspace
commands should follow the same model.

---

## Design Rules

Prefer

* plain text
* simple editing
* keyboard shortcuts
* predictable behavior

Avoid features that distract from programming.

---

## Future

Future versions may include

* Syntax highlighting
* Auto completion
* Code folding
* Multiple tabs
* Assembly workspace mode
* Find and replace
* Go to line

These features should remain optional.

The core editing experience should remain simple.

---

## Compatibility

The Workspace should behave consistently across the C-Series BASIC COMPUTER family whenever practical.

---

Keep Going.
