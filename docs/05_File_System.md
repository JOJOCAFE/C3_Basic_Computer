# C3 BASIC COMPUTER

# 05 File System

Status: Current implementation plus design goals

---

## Purpose

The File System stores programs, data and configuration.

It is designed to be simple, predictable and recoverable.

---

## Design Goals

* Simple
* Reliable
* Human-readable whenever practical
* Easy to explore
* Easy to recover

---

## Storage Layout

The design model divides storage into protected system files and maker
workspace files.

```text
+------------------------------+
| System                       |
+------------------------------+
| Boot Loader                  |
| Runtime                      |
| Shell                        |
| BASIC                        |
| ASM Engine (future)          |
| System Libraries             |
+------------------------------+
| Workspace                    |
+------------------------------+
| basic/                       |
| asm/                         |
| bin/                         |
| config/                      |
| data/                        |
| temp/                        |
+------------------------------+
```

---

## System

The System contains the computer itself.

Examples

* Boot Loader
* Runtime
* Shell
* BASIC Interpreter
* Native ASM Engine (future)
* System Libraries

The System is updated only through the firmware update process.

---

## Workspace

The Workspace contains everything created by the maker.

Current firmware workspace directories

```text
/basic
/asm
/bin
/config
/data
/temp
```

Compatibility aliases may also expose uppercase paths such as `/BASIC`, `/DATA`,
and `/TEMP`. ASM and config workflows are design goals, but their workspace
directories are already part of the standard layout.

The Workspace is fully writable.

---

## Internal File System

Reference implementation:

```text
LittleFS
```

LittleFS is split into two partitions:

- `system_fs` mounted at `/system` (read-mostly system payload boundary)
- `workspace_fs` mounted at `/workspace` (renewable workspace)

`workspace_fs` stores user files and supports `RENEW`.

Boot and protected recovery do not depend on files in `workspace_fs`. If the
workspace is damaged, the shell still starts from the protected system path and
offers `RENEW`.

---

## External Storage

Reference implementation:

```text
microSD

FAT32
```

External storage is optional.

---

## Directory Structure

Current firmware directory set

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

## Path Style

Paths follow Unix conventions.

Current examples

```text
/basic/hello.bas

/data/log.txt
```

Future examples include `/asm/add.asm` and `/config/system.cfg`.

---

## File Types

Suggested conventions

```text
.bas

.asm   future

.bin

.cfg   future

.txt
```

Plain text is preferred whenever practical.

---

## Workspace Ownership

The maker owns the Workspace.

The computer owns the System.

---

## RENEW

RENEW recreates the Workspace.

The System remains unchanged.

RENEW is protected behavior. User files in the Workspace must not be able to
delete, replace or disable it.

Boot does not silently format a damaged Workspace. Recovery is intentional and
goes through the confirmation flow below.

RENEW always displays

```text
WARNING

This operation will remove
all user programs and data.

The system software
will remain unchanged.

Do you understand? (Y/N)
```

If confirmed

```text
Ready to continue? (Y/N)
```

The computer recreates

```text
/basic
/asm
/bin
/config
/data
/temp
```

Then returns to

```text
READY.

>
```

---

## Firmware Update

Firmware updates modify only the System.

The Workspace should remain unchanged whenever practical.

---

## Design Rules

* Keep the structure simple.
* Prefer plain text.
* Use Unix-style paths.
* Use lowercase names.
* Keep the Workspace easy to understand.

---

## Compatibility

The same file structure should be preserved across the C-Series BASIC COMPUTER family whenever practical.

Reference platforms include

* C3 BASIC COMPUTER
* C5 BASIC COMPUTER
* C6 BASIC COMPUTER
* P4 BASIC COMPUTER

---

Keep Going.
