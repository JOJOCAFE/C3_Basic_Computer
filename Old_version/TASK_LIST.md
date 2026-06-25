# ESP32-C3 Computer Task List

This task list is derived from:

- [C3_BASIC_COMPUTER_Design_Freeze_v1.md](./C3_BASIC_COMPUTER_Design_Freeze_v1.md)
- [DesignStrategy.md](./DesignStrategy.md)

The build order follows the incremental plan in the strategy doc and the frozen feature set in the design freeze.

## 1. Phase 1 Core System

Goal: make the ESP32-C3 usable from a PC terminal over USB serial, with a tiny BASIC interpreter and file storage.

Work items:

- Mount LittleFS on boot.
- Create a serial shell reachable from a PC terminal.
- Show a boot banner and `READY.` prompt.
- Implement shell commands:
  - `HELP`
  - `DIR`
  - `LOAD`
  - `SAVE`
  - `DELETE`
  - `NEW`
  - `RUN`
- Add a minimal BASIC interpreter that can run small programs entered from the shell.
- Support at least the BASIC core from the docs:
  - `LET`
  - `PRINT`
  - `INPUT`
  - `IF`
  - `THEN`
  - `ELSE`
  - `FOR`
  - `NEXT`
  - `GOTO`
  - `GOSUB`
  - `RETURN`
  - `END`
- Support the BASIC math functions from the docs:
  - `ABS`
  - `INT`
  - `ROUND`
  - `MIN`
  - `MAX`
  - `SQRT`
  - `SIN`
  - `COS`
  - `TAN`
  - `ASIN`
  - `ACOS`
  - `ATAN`
  - `ATAN2`
  - `LOG`
  - `EXP`
  - `RND`
  - `PI`
  - `E`
  - `DEG`
  - `RAD`
- Make file operations work through the shell so programs can be saved, loaded, listed, and deleted.
- Keep the initial file layout compatible with the strategy doc:
  - `/BASIC`
  - `/ASM`
  - `/CONFIG`
  - `/TEMP`

Acceptance criteria:

- A PC terminal connected over USB serial reaches the shell prompt after boot.
- The shell can list files, save a BASIC program, load it back, and run it.
- A minimal BASIC program can print output through the terminal.
- The filesystem state survives reboot.

## 2. Phase 2 Native Assembly

Goal: run small native RISC-V assembly programs on the ESP32-C3.

Work items:

- Add `ASM` / `ENDASM` source support.
- Add `CALLASM`.
- Build the mini assembler pipeline.
- Execute assembled code natively in IRAM.
- Reject unsupported instructions clearly.

## 3. Phase 3 Monitor Tools

Goal: expose low-level CPU inspection tools.

Work items:

- Add `REG`.
- Add `MEM`.
- Add `DISASM`.
- Add `BREAK`.
- Add `STEP`.
- Add a disassembler for stored machine code.

## 4. Phase 4 Graphics

Goal: enable display output and text/shape drawing.

Work items:

- Drive the ST7796S display.
- Add text console rendering.
- Implement:
  - `CLS`
  - `PSET`
  - `LINE`
  - `RECT`
  - `FRECT`
  - `CIRCLE`
  - `FCIRCLE`
  - `TEXT`
  - `COLOR`

## 5. Phase 5 Audio

Goal: add sound generation on the speaker path.

Work items:

- Drive PWM audio.
- Add `SOUND`.
- Add `PLAY`.
- Verify output through the PAM8302 amplifier.

## 6. Phase 6 Bluetooth Keyboard

Goal: remove the dependency on a PC terminal for normal use.

Work items:

- Pair the BLE HID keyboard.
- Verify text entry into the shell and BASIC editor.
- Confirm compatibility with the Rii 518BT style keyboard path from the design freeze.

## 7. Phase 7 Sensor and GPIO Expansion

Goal: complete the hardware-facing BASIC surface.

Work items:

- Add GPIO aliases and direct pin-number support.
- Implement:
  - `PINMODE`
  - `DWRITE`
  - `DREAD`
- Add LIS3DH support.
- Implement:
  - `ACCELX()`
  - `ACCELY()`
  - `ACCELZ()`

## 8. Phase 8 Standalone System Integration

Goal: combine display, keyboard, audio, filesystem, BASIC, and assembly into one coherent standalone computer.

Work items:

- Verify boot experience.
- Verify shell, BASIC, and storage together.
- Verify graphics and audio do not break terminal mode.
- Confirm the system behaves as a self-contained educational computer.

## 9. Phase 9 RV8 UART Integration

Goal: turn the ESP32-C3 into the terminal/monitor/loader/debug console for RV8-GR.

Work items:

- Add UART bridge mode.
- Add monitor behavior for RV8.
- Add program loader support.
- Add debug console support.

## Immediate Next Step

Phase 1 is hardware-tested:

- USB serial shell and prompt
- LittleFS directory layout
- `DIR`, `SAVE`, `LOAD`, `DELETE`, `NEW`, `LIST`, and `RUN`
- BASIC program entry and execution
- save/load/list/run serial regression
- filesystem persistence across hardware reset

Start Phase 2 with a narrow, auditable boundary:

- capture `ASM` through `ENDASM` source blocks
- store assembly source separately from BASIC source
- reject unsupported instructions with line-specific errors
- do not execute generated code until assembler output is independently tested
