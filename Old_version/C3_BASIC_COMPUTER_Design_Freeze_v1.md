# C3 BASIC COMPUTER
## RISC-V Edition
### Design Freeze v1.0

# Vision

Create a self-contained educational computer based on ESP32-C3.

The system is designed to teach:
- Programming
- Embedded Systems
- File Systems
- Computer Graphics
- Sound Generation
- RISC-V Assembly
- Computer Architecture

The machine must be useful as a standalone computer before any RV8 integration.

# Design Philosophy

The system intentionally avoids:
- Desktop GUI
- Window Manager
- Mouse-centric UI
- Icon-based Launcher
- Desktop Metaphors

Primary user interface:
- Command Shell
- BASIC
- RISC-V Assembly
- Text Console

# Boot Experience

C3 BASIC COMPUTER
RISC-V Edition

READY.

# Hardware Specification (Frozen)

CPU
- ESP32-C3 Super Mini
- RISC-V RV32IMC
- 160 MHz
- ~400 KB SRAM
- 4 MB Flash minimum

Display
- 4.0 inch IPS TFT
- ST7796S
- 480 × 320
- Capacitive Touch
- GT911 preferred

Keyboard
- BLE HID Keyboard
- Rii 518BT compatible

Storage
- microSD
- FAT32

Audio
- 8-bit PWM Audio
- PAM8302 Amplifier
- 8Ω Speaker

Motion Sensor
- LIS3DH

Power
- USB-C
- Single-cell LiPo

# Software

Shell
- HELP
- DIR
- LOAD
- SAVE
- DELETE
- NEW
- RUN

BASIC
- LET
- PRINT
- INPUT
- IF
- THEN
- ELSE
- FOR
- NEXT
- GOTO
- GOSUB
- RETURN
- END

Graphics
- CLS
- PSET
- LINE
- RECT
- FRECT
- CIRCLE
- FCIRCLE
- TEXT
- COLOR

Audio
- SOUND frequency,duration,amplitude
- PLAY

GPIO
- PINMODE
- DWRITE
- DREAD

Sensors
- ACCELX()
- ACCELY()
- ACCELZ()

RISC-V
- Native Assembly
- Assembler
- Disassembler
- Register Viewer
- Memory Viewer
- Safe Sandbox

# Educational Path

1. BASIC
2. Embedded Programming
3. RISC-V Assembly
4. Native Machine Code

# Future

RV8 integration via UART:
- Terminal
- Monitor
- Program Loader
- Debug Console
