# C3 BASIC COMPUTER

# 06 BASIC Language

Status: Implemented first tiny numbered BASIC slice.

---

## Purpose

C3 BASIC is the small programming language surface for C3 BASIC COMPUTER. The
current implementation is intentionally tiny, deterministic, and line-numbered.
Larger modern BASIC features are future goals, not current behavior.

Programs are stored as UTF-8 plain text in `/basic/*.bas` and edited with:

```text
BASIC /basic/name.bas
```

Inside nano BASIC mode:

```text
:run    Save, validate, and run the current program
:debug  Save, validate, and step-run the current program
```

The boot shell does not expose BASIC immediate mode. Commands such as `PRINT`,
`RUN`, `LIST`, `LOAD`, `SAVE`, and `NEW` are rejected by the boot shell.

---

## Current Program Format

Only classic numbered BASIC is executable in the current firmware:

```basic
10 PRINT "HELLO, WORLD!"
20 END
```

Rules:

- Each nonblank executable source line must start with a positive decimal line
  number.
- The line number must be followed by whitespace or end-of-line.
- Malformed numeric prefixes such as `123ABC` are rejected before save/run.
- The nano BASIC buffer is 64 KiB.
- Validation errors include source line number, reason, and a short excerpt.

Modern unnumbered BASIC is deferred.

---

## Variables And Expressions

Current variables are single letters `A` through `Z`.

Supported expression features:

```text
+  -  *  /  %
( )
=  <>  <  <=  >  >=
```

Built-in functions:

```text
ABS(expr)
INT(expr)
RND()
RND(limit)
DREAD(pin)
AREAD(gpio)
```

Current constants:

```text
LOW
HIGH
INPUT
INPUT_PULLUP
INPUT_PULLDOWN
OUTPUT
OUTPUT_OPEN_DRAIN
```

Long variable names, arrays, floating point, strings as variables, `PI`, `E`,
trigonometry, logarithms, and modern structured control syntax are deferred.

---

## Current Statements

Core statements:

```text
REM text
PRINT expr
PRINT "text"
LET A = expr
A = expr
INPUT A
IF expr relop expr THEN line [ELSE line]
GOTO line
GOSUB line
RETURN
FOR A = start TO limit [STEP step]
NEXT [A]
END
STOP
```

Safe service bridge:

```basic
SHELL "PWD"
SHELL "CAT /data/note.txt"
HARDWARE "gpio read -p 8"
HARDWARE "adc read -p 0"
```

Typed hardware statements:

```basic
PINMODE 8, OUTPUT
DWRITE 8, HIGH
DTOGGLE 8
```

Typed hardware functions:

```basic
PRINT DREAD(8)
PRINT AREAD(0)
```

Hardware errors stop the program and include the BASIC line number:

```text
Line 10: protected pin
BASIC ERROR
```

---

## Debug Mode

`:debug` step-runs a BASIC program. It is BASIC statement stepping, not CPU or
native monitor stepping.

Controls:

```text
Enter or s  Step next statement
c           Continue until END or error
p           Print current variables
l           List current source line
q           Quit debug and return to nano
```

---

## Deferred Language Goals

These remain design goals and must be implemented as separate milestones:

- Modern unnumbered BASIC.
- Long identifiers.
- Arrays and `DIM`.
- String variables and string functions.
- `DATA` / `READ`.
- File I/O from BASIC beyond the current controlled `SHELL "CAT ..."` bridge.
- Graphics, sound, network, timers, events, I2C, and SPI BASIC libraries.
- Native ASM capture/execution.

BASIC cannot invoke native execution. Guarded C3COM execution is available only
from the boot shell through explicit `RUN /bin/name.com [args...]`, with C3COM
header and CRC validation.
