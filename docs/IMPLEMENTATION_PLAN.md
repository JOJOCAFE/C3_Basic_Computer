# C3 BASIC COMPUTER

# IMPLEMENTATION PLAN

Status: Review

---

## Objective

Implement C3 BASIC COMPUTER incrementally.

Each milestone produces a usable computer.

Every milestone should be independently usable, testable and demonstrable.

---

# Milestone 0

## Repository Foundation

### Goal

Prepare the project.

### Tasks

* Repository
* Documentation
* Build System
* Development Environment

### Done

The project is ready for implementation.

---

# Milestone 1

## Shell

### Goal

Create a usable computer shell.

### Hardware

* ESP32-C3 Super Mini
* USB Serial
* PC Terminal

### Tasks

* Boot
* READY.
* Prompt
* help
* pwd
* dir
* ls
* cd
* cat
* write
* mkdir
* delete
* rm
* copy
* cp
* move
* mv

### Done

The computer boots into a usable shell, manages files inside the workspace, and
keeps BASIC/RENEW behavior stable.

---

# Milestone 2

## File System

### Goal

Persistent storage.

### Tasks

* LittleFS
* Workspace
* Files
* Directories
* Configuration

### Done

Programs and files survive reboot.

---

# Milestone 3

## Workspace

### Goal

Create a programming workspace.

### Tasks

* edit
* save
* load
* find
* goto line
* run
* asm

### Done

Programs can be created, edited and executed.

---

# Milestone 4

## BASIC

### Goal

Minimal BASIC.

### Tasks

* Parser
* Interpreter
* Variables
* PRINT
* INPUT
* IF
* FOR
* GOTO
* GOSUB

### Done

Classic BASIC programs execute.

---

# Milestone 5

## Hardware Service Boundary

### Goal

Expose board hardware through reusable firmware services without adding
hardware code to the shell core.

### Status

Completed for the `/bin/hardware` GPIO/ADC/I2C/SPI terminal adapter boundary.
GPIO is board-tested; ADC/I2C/SPI are build-verified and ready for board smoke.

### Done

* `source/hardware` provides GPIO, ADC, I2C and SPI C APIs.
* `source/bin` registers `/bin/hardware` GPIO, ADC, I2C, and SPI adapters in
  the root firmware.
* `source/shell` remains standalone without hardware/bin dependencies.
* GPIO8 board test passed through the shell using generic GPIO commands.
* Root firmware and standalone shell builds pass with the hardware adapters
  wired.

### Deferred

* BASIC hardware statements should call `source/hardware`.
* ADC/I2C/SPI board smoke should be run after flashing the wired adapters.

---

# Milestone 6

## BASIC Library

### Goal

Core BASIC Library.

### Tasks

* Core
* Math
* Files
* GPIO
* System
* Assembly

### Done

The BASIC Library is operational.

---

# Milestone 7

## Native RISC-V Assembly

### Goal

Execute native RISC-V code.

### Tasks

* Parser
* Assembler
* CALLASM()
* Standalone ASM
* Native execution

### Done

Assembly programs execute on the CPU.

---

# Milestone 7

## BASIC Library Extension

### Goal

BASIC Library Extension capabilities.

### Tasks

Graphics

* CLS
* COLOR
* TEXT
* LINE
* RECT
* CIRCLE

Sound

* BEEP
* SOUND
* PLAY

Motion

* ACCELX()
* ACCELY()
* ACCELZ()

### Done

Extended BASIC is operational.

---

# Milestone 8

## Standalone Computer

### Goal

Replace the PC terminal.

### Hardware

* ST7796
* BLE Keyboard
* PAM8302
* LIS3DH

### Done

The computer operates standalone.

---

# Milestone 9

## System

### Goal

Complete the computer.

### Tasks

* RENEW
* UPDATE
* Diagnostics
* Recovery

### Done

The computer is recoverable.

---

# Milestone 10

## Version 1.0

### Goal

Release.

### Tasks

* Documentation
* Examples
* Testing
* Optimization

### Done

Version 1.0 released.

---

# Development Rules

* One milestone at a time.
* One feature at a time.
* One commit at a time.
* Documentation first.
* Every milestone must remain operational.

---

# Demonstration Rule

A milestone is not complete until it can be demonstrated.

Examples

Milestone 1

```text
READY.

>

help

ls
```

Milestone 4

```basic
PRINT "HELLO, WORLD!"
```

Milestone 6

```basic
PRINT CALLASM("ADD2",10,20)
```

---

# Definition of Done

A milestone is complete when

* Implementation is finished.
* Documentation matches implementation.
* Manual testing passes.
* Existing milestones continue to work.

---

Keep Going.
