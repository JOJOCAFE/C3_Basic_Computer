# C3 BASIC COMPUTER

# 08 Workspace

Status: Draft

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

The Workspace opens all supported text files.

Examples

```text id="t2i06x"
.bas

.asm

.txt

.cfg
```

Every supported file uses the same editor.

---

## Plain Text

All editable files are UTF-8 plain text.

The Workspace never stores proprietary editor formats.

---

## Primary Actions

```text id="oh2b8c"
Open

Save

Save As

Close

Run

Assemble

Help

Find

Go To Line
```

---

## Running Programs

When the current file is

```text id="4u18g4"
.bas
```

the command

```text id="pmrbp9"
run
```

executes the current program.

---

## Assembling Programs

When the current file is

```text id="fjqt2u"
.asm
```

the command

```text id="rprn1g"
asm
```

assembles the current program.

---

## HELP Integration

HELP is available directly from the Workspace.

Examples

```text id="q29k08"
help line

help sound

help asm
```

The maker should never leave the Workspace to look for documentation.

---

## Keyboard

The Workspace is designed for keyboard operation.

Mouse or touch support may be added in future versions.

Keyboard remains the primary interface.

---

## Editing

Minimum editing capabilities

```text id="r4e5ep"
Insert

Delete

Copy

Paste

Undo

Redo

Find

Replace
```

---

## Navigation

Examples

```text id="dgkvlg"
Go To Line

Beginning of File

End of File

Page Up

Page Down
```

---

## Language Independence

The Workspace is not a BASIC editor.

The Workspace is not an Assembly editor.

It edits text.

The file determines how it is used.

---

## Discoverability

The Workspace should encourage exploration.

Examples

```text id="k2gnyb"
help

run

asm
```

should always be available without leaving the current file.

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

These features should remain optional.

The core editing experience should remain simple.

---

## Compatibility

The Workspace should behave consistently across the C-Series BASIC COMPUTER family whenever practical.

---

Keep Going.
