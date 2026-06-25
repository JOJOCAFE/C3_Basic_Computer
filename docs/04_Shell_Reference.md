# C3 BASIC COMPUTER

# 04 Shell Reference

Status: Review

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
pwd
```

to display it.

---

## Command Style

The Shell follows Unix-style conventions whenever practical.

Rules

* lowercase commands
* case-sensitive
* space-separated arguments
* Unix-style paths

Examples

```text
ls

cd basic

cat hello.bas

edit hello.bas

run
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

Shell commands are organized into four groups.

```text
Core

Workspace

System

Monitor
```

---

## Core Commands

```text
help

pwd

ls

cd

cat

cp

mv

rm

mkdir

rmdir
```

---

## Workspace Commands

```text
edit

run

basic

asm
```

Examples

```text
> edit hello.bas

> run

> asm add.asm
```

When no filename is supplied, `run` executes the current program currently open in the Workspace whenever practical.

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

They are intended for exploration, debugging and learning.

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
