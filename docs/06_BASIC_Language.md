# C3 BASIC COMPUTER

# 06 BASIC Language

Status: Draft

---

## Purpose

C3 BASIC is the primary programming language of C3 BASIC COMPUTER.

It combines the simplicity of classic BASIC with the capabilities of a modern computer.

Classic BASIC.

Modern Computer.

---

## Language Goals

The language should be

* Easy to read
* Easy to write
* Easy to explore
* Easy to modify
* Easy to share

Programs are stored as UTF-8 plain text.

---

## Program Format

Both formats are supported.

Classic

```basic
10 PRINT "HELLO, WORLD!"
20 END
```

Modern

```basic
PRINT "HELLO, WORLD!"
END
```

Editors may convert between formats whenever practical.

---

## Case Sensitivity

The language is case-sensitive.

Examples

```basic
score

Score

SCORE
```

are three different identifiers.

---

## Variables

Variable names are not limited to single letters.

Examples

```basic
score

playerName

total_sum

counter1
```

---

## Assignment

Both forms are valid.

```basic
LET score = 100
```

```basic
score = 100
```

---

## Comments

Two comment styles are supported.

```basic
REM This is a comment
```

```basic
' This is also a comment
```

---

## Strings

Strings use double quotation marks.

Example

```basic
PRINT "HELLO, WORLD!"
```

---

## Control Flow

Conditional

```basic
IF score > 90 THEN
    PRINT "PASS"
ELSE
    PRINT "TRY AGAIN"
ENDIF
```

Loop

```basic
FOR i = 1 TO 10
    PRINT i
NEXT
```

---

## Arrays

Example

```basic
DIM score(100)
```

---

## Built-in Functions

Examples

```text
ABS
INT
ROUND

SIN
COS
TAN

SQRT

LOG
EXP

RND
```

Constants

```text
PI

E
```

---

## Program Files

Programs use

```text
.bas
```

UTF-8 plain text.

---

## Design Rules

The language prefers readability over brevity.

Plain text over tokenized storage.

Simple syntax over clever syntax.

Classic ideas over unnecessary novelty.

---

Keep Going.
