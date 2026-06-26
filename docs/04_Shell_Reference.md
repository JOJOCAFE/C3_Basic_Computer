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

The Shell follows simple C3 command conventions now and adds Unix-style aliases
where they make the workspace easier to use.

Rules

* document C3 commands in uppercase first
* accept command keywords case-insensitively in firmware
* space-separated arguments
* Unix-style paths
* keep file commands inside `/workspace`

Examples

```text
LS

CD basic

CAT hello.bas

WRITE temp/note.txt hello

RUN
```

---

## Workspace

The Shell works inside the Workspace.

Default structure

```text
/

basic/
asm/
bin/
config/
data/
temp/
```

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

DIR

LS

CD

MKDIR

CAT

WRITE

DELETE

RM

COPY

CP

MOVE

MV
```

---

## Workspace Commands

Current BASIC workspace commands preserved by the shell:

```text
NEW

LIST

RUN

LOAD

SAVE
```

Deferred workspace commands:

```text
EDIT

BASIC

ASM
```

Examples

```text
> SAVE basic/hello.bas

> LOAD basic/hello.bas

> RUN
```

ASM capture is the next candidate milestone. Native execution remains blocked
until a later guarded runtime sprint.

---

## System Commands

```text
version

memory

date

time

renew

update

diagnostics
```

These commands provide information about the computer and manage the system.
Only implemented commands should appear in firmware `HELP`; broader system
commands are deferred unless listed in Sprint 002.

Detailed behavior is documented in **10 System Commands**.

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

HELP is the built-in reference system.

Every public command supports

```text
help <command>
```

Examples

```text
help

help ls

help run

help version

help reg

help graphics

help sound

help asm
```

Example

```text
> help ls

ls

List files in the current directory.

Example

ls

See also

pwd
cd
```

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
