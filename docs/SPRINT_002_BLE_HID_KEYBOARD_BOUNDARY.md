# Sprint 002 BLE HID Keyboard Boundary

Status: Hardware pending

## Decision

C3 BASIC COMPUTER needs a BLE HID host backend for a real BLE keyboard.

The local macro-pad reference is useful as an ESP32-C3 BLE HID device example,
but it is not the direct driver shape for C3 input. C3 is the computer, so it
must receive keyboard reports rather than advertise itself as a keyboard.

## References

```text
/home/jo/Codex/ESP32-C3-BLE-Media-Macro-Pad
/home/jo/esp-idf/examples/bluetooth/esp_hid_host
/home/jo/esp-idf/components/esp_hid
```

## Current Implementation

Files:

```text
main/input.h
main/input_serial.c
main/input_ble_hid.c
source/shell/shell.c
main/CMakeLists.txt
```

Current active backend:

```text
USB Serial/JTAG
PC terminal
```

BLE HID backend state:

```text
driver boundary present
boot-keyboard ASCII mapper present
Bluetooth stack integration not enabled yet
real keyboard pairing not tested yet
```

## Input Boundary

The shell talks only to:

```text
input_init()
input_read_line()
input_write()
input_write_bytes()
```

The shell does not call BLE APIs directly.

## BLE Host Work Still Needed

When the real keyboard is available:

1. Enable the ESP-IDF BLE HID host configuration for ESP32-C3.
2. Initialize `esp_hid` / `esp_hidh` from the input backend, not from shell.
3. Scan for BLE HID keyboards.
4. Pair and reconnect safely.
5. Convert HID reports into `input_event_t`.
6. Feed printable keys, Enter, Backspace and arrows into the same line editor.
7. Keep USB Serial/JTAG available as the recovery/development input path.

## Acceptance Before Hardware

1. Firmware builds with the input boundary.
2. PC terminal behavior is unchanged.
3. BLE code is isolated from shell logic.
4. No public shell command depends on BLE being available.

## Acceptance With Hardware

1. A BLE keyboard can pair.
2. Typing text enters shell commands.
3. Enter submits a command.
4. Backspace edits the current line.
5. Disconnect/reconnect does not crash the shell.
6. USB Serial/JTAG still works after BLE failure.
