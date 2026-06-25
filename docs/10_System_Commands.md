# C3 BASIC COMPUTER

# 10 System Commands

Status: Review

---

## Purpose

System Commands provide information and management of C3 BASIC COMPUTER.

They describe the computer itself.

They are not part of the BASIC language.

---

## Design Goals

System Commands should remain

* Small
* Predictable
* Recoverable
* Easy to remember

---

## Command Style

System Commands

* use lowercase
* use complete words
* avoid abbreviations whenever practical

Examples

```text id="ocpjlwm"
version

memory

date

time

renew

update

diagnostics
```

---

## Command Summary

### Information

```text id="jlwm31"
version

memory

date

time
```

---

### Maintenance

```text id="jlwm32"
renew

update
```

---

### Diagnostics

```text id="jlwm33"
diagnostics
```

---

## version

Displays computer information.

Example

```text id="jlwm34"
> version
```

Output

```text id="jlwm35"
C3 BASIC COMPUTER

Version 0.1
```

---

## memory

Displays memory information.

Example

```text id="jlwm36"
> memory
```

Output

```text id="jlwm37"
Flash

Workspace

Free RAM
```

---

## date

Displays the current date.

---

## time

Displays the current time.

---

## renew

Recreates the Workspace.

The System remains unchanged.

The command always displays

```text id="jlwm38"
WARNING.

This operation removes
all user programs and data.

The system software
will remain unchanged.

Do you understand? (Y/N)
```

If confirmed

```text id="jlwm39"
Ready to continue? (Y/N)
```

The computer recreates the Workspace.

Then returns to

```text id="jlwm40"
READY.

>
```

---

## update

Updates the System software.

Firmware updates modify only the System.

The Workspace should remain unchanged whenever practical.

---

## diagnostics

Runs built-in diagnostics.

The command checks the health of the computer.

Diagnostics never modify user data.

---

## HELP

Every System Command supports

```text id="jlwm41"
help version

help memory

help renew

help diagnostics
```

---

## Relationship to Monitor Commands

System Commands manage the computer.

Monitor Commands inspect the computer.

Examples of Monitor Commands

```text id="jlwm42"
reg

mem

dump

disasm

step

break
```

Monitor Commands are documented separately.

---

## Design Rules

System Commands should

* provide information
* manage the computer
* avoid unnecessary complexity

They should never become part of the BASIC language.

---

## Compatibility

System Commands should remain compatible across the C-Series BASIC COMPUTER family whenever practical.

Reference platforms include

* C3 BASIC COMPUTER
* C5 BASIC COMPUTER
* C6 BASIC COMPUTER
* P4 BASIC COMPUTER

---

Keep Going.
