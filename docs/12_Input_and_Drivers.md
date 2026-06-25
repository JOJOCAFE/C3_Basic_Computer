# C3 BASIC COMPUTER

# 12 Input and Drivers

Status: Draft

---

## Purpose

Input and Drivers define how external hardware connects to C3 BASIC COMPUTER.

The first required input device is the keyboard.

Keyboard input must work consistently across Shell, Workspace, BASIC and Assembly.

---

## Design Goals

Input should be

* Simple
* Predictable
* Shared
* Replaceable
* Independent from applications

Applications should not read hardware directly.

---

## Input Model

All input devices produce Key Events.

```text
Input Device

↓

Input Driver

↓

Key Event

↓

Input Queue

↓

Shell / Workspace / BASIC
```

---

## Phase 1 Input

Phase 1 uses PC Terminal input.

```text
PC Terminal

↓

USB Serial

↓

Input Queue

↓

Shell
```

This allows development before standalone hardware is complete.

---

## Phase 2 Input

Phase 2 adds Bluetooth keyboard support.

```text
BLE HID Keyboard

↓

BLE HID Host

↓

Key Mapper

↓

Input Queue

↓

Shell / Workspace / BASIC
```

---

## Keyboard Requirement

The reference standalone keyboard must be

```text
BLE HID Keyboard
```

Bluetooth Classic keyboards are not part of the reference design for ESP32-C3.

---

## Key Event

A Key Event should contain

```text
key

modifier

pressed / released
```

Examples

```text
A

Enter

Backspace

Ctrl-S

Ctrl-Q
```

---

## Input Queue

The Input Queue stores Key Events before they are consumed.

Consumers include

```text
Shell

Workspace

BASIC INPUT

Monitor Commands
```

---

## Key Mapping

The Key Mapper converts raw keyboard events into C3 BASIC COMPUTER key events.

Responsibilities

* printable characters
* Enter
* Backspace
* Arrow keys
* Function keys
* Ctrl combinations

---

## Workspace Shortcuts

Initial shortcuts

```text
Ctrl-S    Save

Ctrl-Q    Quit

Ctrl-F    Find

Ctrl-G    Go To Line
```

These shortcuts may evolve as the Workspace matures.

---

## Driver Rule

Drivers belong below the input model.

Shell, Workspace and BASIC should never depend on a specific keyboard driver.

---

## Future Input

Future input devices may include

```text
Touch

GPIO buttons

USB keyboard

Serial console

Game controller
```

These devices should also produce Key Events whenever practical.

---

## Design Rules

Keep input simple.

Keep keyboard behavior consistent.

Avoid application-specific keyboard handling whenever practical.

The same key should mean the same thing across the computer unless there is a clear reason.

---

## Compatibility

Input behavior should remain consistent across the C-Series BASIC COMPUTER family whenever practical.

Reference platforms include

* C3 BASIC COMPUTER
* C5 BASIC COMPUTER
* C6 BASIC COMPUTER
* P4 BASIC COMPUTER

---

Keep Going.
