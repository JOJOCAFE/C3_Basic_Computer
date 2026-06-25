# C3 BASIC COMPUTER

# 09 Native RISC-V Assembly

Status: Review

---

## Purpose

Native RISC-V Assembly is a first-class capability of C3 BASIC COMPUTER.

Makers write, assemble and execute real RISC-V code on the reference platform.

This is native execution.

Not simulation.

---

## Design Goals

Native Assembly should remain

* Real
* Small
* Discoverable
* Inspectable
* Recoverable

---

## Architecture

Three layers are involved.

```text id="62r6ik"
RISC-V ISA

↓

Platform

↓

C3 BASIC COMPUTER Runtime
```

---

## RISC-V ISA

The instruction set follows standard RISC-V whenever practical.

Examples

```asm id="ukg03h"
add
sub
addi
and
or
xor
lw
sw
beq
jal
ret
```

Platform-independent examples should remain portable whenever practical.

---

## Platform

Reference platform

```text id="crnt1h"
ESP32-C3

RV32IMC
```

Platform-specific behavior belongs here.

Examples

* Memory layout
* Flash
* IRAM
* DRAM
* Peripherals

---

## Runtime

The Runtime provides services to Assembly programs.

Examples

* Files
* Graphics
* Sound
* GPIO
* System

These services are specific to C3 BASIC COMPUTER.

They are not part of standard RISC-V.

---

## Assembly Source

Assembly source files use

```text id="6r83tt"
.asm
```

Encoding

```text id="vxh4n9"
UTF-8
```

---

## Inline Assembly

Assembly may be embedded inside BASIC.

Example

```basic id="m7jspl"
ASM ADD2
    add a0,a0,a1
    ret
ENDASM

PRINT CALLASM("ADD2",10,20)
```

---

## Standalone Assembly

Assembly programs may also exist as standalone

```text id="l75hgo"
.asm
```

files.

---

## Assembly Workflow

```text id="0bjlwm"
Source

↓

Assembler

↓

Machine Code

↓

Native Execution

↓

Return
```

---

## Calling Convention

CALLASM()

Arguments

```text id="2lh1o5"
a0

a1

a2

a3
```

Return value

```text id="g8q4t0"
a0
```

---

## Assembly Tools

Assembly tools are provided by the Shell.

Examples

```text id="nlnq3u"
asm

disasm

reg

mem

dump

step

break
```

These commands are documented in the Shell and Monitor references.

---

## Safety

Assembly programs may

* crash
* hang
* corrupt the Workspace

The System remains recoverable.

---

## Design Rules

Prefer standard RISC-V.

Avoid unnecessary platform-specific extensions.

Keep Assembly close to the architecture.

Keep Runtime APIs separate from the instruction set.

---

## Compatibility

Assembly source should remain compatible across the C-Series BASIC COMPUTER family whenever practical.

Reference platforms include

* C3 BASIC COMPUTER
* C5 BASIC COMPUTER
* C6 BASIC COMPUTER
* P4 BASIC COMPUTER

---

Keep Going.
