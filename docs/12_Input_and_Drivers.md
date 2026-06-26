# C3 BASIC COMPUTER

# 12 Input and Drivers

Status: Review, Sprint 002 input boundary implemented

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

Phase 1 uses PC Terminal input. In the current firmware, this path is routed
through the shared input service.

```text
PC Terminal

↓

USB Serial/JTAG

↓

Input Queue

↓

Shell
```

This allows development before standalone hardware is complete.

---

## Phase 2 Input

Phase 2 adds Bluetooth keyboard support. The firmware has a compiled BLE HID
backend boundary and boot-keyboard ASCII mapper, but real pairing is pending
keyboard hardware.

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

## Hardware Service Boundary

General board hardware access is separate from keyboard input.

Current implementation:

```text
source/hardware

GPIO / ADC / I2C / SPI C service APIs

source/bin

/bin/hardware terminal command adapters
```

The shell can call `/bin/hardware` as a terminal command, but the hardware
code is not part of the shell core. BASIC GPIO/ADC statements call the same
`source/hardware` APIs through `main/basic_hardware.*`. Future monitor code
should also call the C APIs directly unless a text command path is required.

The current hardware terminal command families are:

```text
/bin/hardware gpio in -p <gpio>
/bin/hardware gpio out -p <gpio> [-v 0|1]
/bin/hardware gpio read -p <gpio>
/bin/hardware gpio write -p <gpio> -v 0|1
/bin/hardware gpio toggle -p <gpio>
/bin/hardware adc read -p <gpio>
/bin/hardware i2c config -sda <gpio> -scl <gpio> [-f <hz>] [--pullups]
/bin/hardware i2c probe -a <addr>
/bin/hardware i2c scan
/bin/hardware spi config -mosi <gpio> -miso <gpio> -sclk <gpio> [-cs <gpio>] [-f <hz>] [-m <mode>]
/bin/hardware spi xfer -tx <hexbytes>
```

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
