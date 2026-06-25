# C3 BASIC COMPUTER

# 02 Software Architecture

Status: Review

---

## Overview

C3 BASIC COMPUTER is designed as a computer, not as firmware.

The software architecture is organized in independent layers.

Each layer has a single responsibility.

Upper layers should not depend on implementation details.

Lower layers should never know how they are used.

---

## Layer Overview

```text
+----------------------------------+
| Maker Programs                   |
+----------------------------------+
| BASIC Library           |
+----------------------------------+
| BASIC Language                   |
+----------------------------------+
| Native RISC-V ASM Engine         |
+----------------------------------+
| Editor / Workspace               |
+----------------------------------+
| Command Shell                    |
+----------------------------------+
| Storage Services                 |
| Hardware Services                |
| System Services                  |
+----------------------------------+
| Platform Runtime                 |
+----------------------------------+
```

---

## Platform Runtime

Platform-specific implementation.

Reference implementation:

* ESP32-C3 Super Mini
* ESP-IDF
* Drivers
* Memory
* Hardware initialization

Future C5, C6 and P4 implementations should modify this layer only whenever practical.

---

## Hardware Services

Hardware-independent services.

Examples

* Display
* Audio
* GPIO
* Timer
* Random
* Serial
* Network

Applications never access hardware directly.

---

## Storage Services

Persistent storage.

Reference implementation

* Internal Workspace: LittleFS
* External Storage: FAT32 (microSD)

Responsibilities

* Files
* Directories
* Workspace
* Configuration
* Program storage

---

## System Services

Provides computer-wide services.

Examples

* Version
* Time
* Memory information
* Firmware update
* Recovery
* Diagnostics

---

## Command Shell

The Shell is the default environment.

Responsibilities

* Command execution
* Navigation
* Program launching
* System utilities

The Shell follows Unix-like conventions whenever practical.

---

## Editor / Workspace

The Editor is the maker's primary workspace.

Responsibilities

* Text editing
* Program editing
* File editing
* Search
* Save
* Run
* Assemble

The Editor is language-independent.

It is used for BASIC, Assembly and plain text.

---

## Native RISC-V ASM Engine

Provides native RISC-V development.

Responsibilities

* Parser
* Assembler
* Machine code generation
* Native execution
* Disassembler

This is a defining capability of C3 BASIC COMPUTER.

---

## BASIC Language

Defines the BASIC language.

Responsibilities

* Syntax
* Statements
* Expressions
* Program structure

The language does not directly access hardware.

---

## BASIC Library

Provides the built-in capabilities of the language.

Examples

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

---

## Maker Programs

Everything created by the maker belongs here.

Examples

* BASIC programs
* Assembly programs
* Libraries
* Utilities
* Games

---

## Architectural Rules

### Separation of Concerns

Each layer has one responsibility.

---

### Stable Interfaces

Layers communicate only through documented interfaces.

---

### Platform Independence

Architecture describes concepts.

Platform Runtime describes implementation.

---

### Recoverability

System software remains protected.

Workspace is renewable.

---

### Progressive Learning

Each layer reveals another level of the computer.

The architecture itself is part of the learning experience.

---

## Reference Platform

Reference implementation

ESP32-C3 Super Mini

Future C-Series computers should preserve this architecture whenever practical.

* C3 BASIC COMPUTER
* C5 BASIC COMPUTER
* C6 BASIC COMPUTER
* P4 BASIC COMPUTER

Keep Going.
