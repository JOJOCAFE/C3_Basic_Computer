# C3 BASIC COMPUTER

# 07 BASIC Library

Status: Review

---

## Purpose

The BASIC Library provides the built-in capabilities of C3 BASIC.

The BASIC language defines syntax.

The BASIC Library defines functionality.

Every public capability should be discoverable through HELP.

---

## Library Organization

```text
Core

Math

Files

Graphics

Sound

Motion

GPIO

Network

System

Assembly
```

---

## Core

Statements

```text
PRINT

INPUT

END

STOP

PAUSE

WAIT
```

---

## Math

Functions

```text
ABS()

INT()

ROUND()

MIN()

MAX()

SQRT()

SIN()

COS()

TAN()

ASIN()

ACOS()

ATAN()

ATAN2()

LOG()

EXP()

RND()
```

Constants

```text
PI

E
```

---

## Files

Functions

```text
OPEN()

CLOSE()

READ()

WRITE()

APPEND()

DELETE()

RENAME()

EXISTS()

SIZE()
```

---

## Graphics

Statements

```text
CLS

COLOR

TEXT

PSET

LINE

RECT

FRECT

CIRCLE

FCIRCLE
```

Functions

```text
POINT()
```

---

## Sound

Statements

```text
BEEP

SOUND

PLAY
```

Future

```text
BGSOUND

PCM
```

---

## Motion

Reference sensor

```text
LIS3DH
```

Functions

```text
ACCELX()

ACCELY()

ACCELZ()
```

---

## GPIO

Statements

```text
PINMODE

DWRITE

DREAD
```

Functions

```text
AREAD()

AWRITE()
```

GPIO may be referenced by

Pin number

```basic
DWRITE 8,1
```

or Alias

```basic
DWRITE "LED",1
```

---

## Network

Future module

```text
WIFI()

HTTP()

TCP()

UDP()
```

---

## System

Functions

```text
VERSION()

MEMORY()

TIME()

DATE()
```

---

## Assembly

Statements

```text
ASM

ENDASM
```

Functions

```text
CALLASM()
```

Assembly is a first-class capability of C3 BASIC COMPUTER.

Assembly tools

```text
asm

disasm

reg

mem

dump

step

break
```

are Shell Monitor Commands.

---

## Naming Convention

Statements perform actions.

Examples

```text
PRINT

LINE

SOUND
```

Functions return values.

Examples

```text
SIN()

RND()

POINT()

VERSION()

ACCELX()
```

---

## HELP

Every public library capability should be discoverable through HELP.

Examples

```text
help graphics

help sound

help motion

help gpio

help line

help accelx
```

---

## Design Rules

The BASIC Library should remain

* Small
* Consistent
* Readable
* Discoverable

Prefer extending existing concepts over introducing new syntax.

---

## Compatibility

The BASIC Library should remain compatible across the C-Series BASIC COMPUTER family whenever practical.

Reference platforms include

* C3 BASIC COMPUTER
* C5 BASIC COMPUTER
* C6 BASIC COMPUTER
* P4 BASIC COMPUTER

---

Keep Going.
