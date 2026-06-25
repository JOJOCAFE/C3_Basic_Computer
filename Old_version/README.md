# ESP32-C3 Computer

ESP32-C3 firmware for a tiny BASIC computer over USB serial.

Current scope:

- USB serial shell
- LittleFS storage
- BASIC program entry, save/load, list, and run
- BASIC statements: `REM`, `LET`, assignment, `PRINT`, `INPUT`, `IF ... THEN ... ELSE`,
  `FOR ... TO ... STEP`, `NEXT`, `GOTO`, `GOSUB`, `RETURN`, `END`
- BASIC math functions: `ABS`, `INT`, `ROUND`, `MIN`, `MAX`, `SQRT`, `SIN`, `COS`, `TAN`, `ASIN`, `ACOS`, `ATAN`, `ATAN2`, `LOG`, `EXP`, `RND`, `PI`, `E`, `DEG`, `RAD`
- shell commands: `HELP`, `DIR`, `LOAD`, `SAVE`, `DELETE`, `NEW`, `LIST`, `RUN`, `RENEW`

## Hardware

- Board: ESP32-C3
- Serial interface: Espressif USB JTAG/serial debug unit
- Common serial port examples:
  - Windows: `COM7`
  - Linux: `/dev/ttyACM0`

## Prerequisites

- ESP-IDF 5.3.x
- Python 3
- `pyserial`

## Build

Use the version-guarded launcher from the project root. It defaults to
`$HOME/esp-idf`; set `IDF53_PATH` if ESP-IDF 5.3 is installed elsewhere.

```bash
tools/idf53.sh -B build-idf53 build
```

The launcher rejects other ESP-IDF versions before CMake can mix toolchains in
an existing build directory.

## Flash

Example:

```bash
tools/idf53.sh -B build-idf53 -p COM7 flash
```

On Linux:

```bash
tools/idf53.sh -B build-idf53 -p /dev/ttyACM0 flash
```

## Smoke test

The host-side serial smoke test exercises `DIR`, `SAVE`, and `DELETE` on a disposable BASIC file.

Windows:

```powershell
python .\tools\dir_delete_smoke.py --port COM7
```

Linux:

```bash
python3 tools/dir_delete_smoke.py --port /dev/ttyACM0
```

The load/list/run regression exercises a saved BASIC program end to end.

Windows:

```powershell
python .\tools\load_list_run_smoke.py --port COM7
```

Linux:

```bash
python3 tools/load_list_run_smoke.py --port /dev/ttyACM0
```

The reboot-persistence regression saves a disposable BASIC program, resets the
board through USB control lines, reloads and runs the program, then deletes it:

```bash
python3 tools/reboot_persistence_smoke.py --port /dev/ttyACM0
```

## What the shell does

After boot you should see:

```text
C3 BASIC COMPUTER
Version 0.1

READY.
> 
```

Example session:

```text
HELP
10 PRINT "HELLO"
RUN
SAVE TEST
NEW
LOAD TEST
LIST
DELETE TEST
```

## BASIC command reference

Numbered lines are stored in the current program. Entering a line number with
no statement deletes that line. Commands and keywords are case-insensitive.

| Statement | Form |
| --- | --- |
| Comment | `REM text` |
| Assignment | `LET A = expression` or `A = expression` |
| Output | `PRINT "text"` or `PRINT expression` |
| Input | `INPUT A` |
| Conditional branch | `IF expression = expression THEN 100 ELSE 200` |
| Loop | `FOR I = 1 TO 10 STEP 1` and `NEXT I` |
| Branch | `GOTO 100` |
| Subroutine | `GOSUB 100`, then `RETURN` |
| Stop | `END` |

Expressions support variables, integer constants, parentheses, `+`, `-`, `*`,
`/`, and `%`. Conditions support `=`, `<>`, `<=`, `>=`, `<`, and `>`.

Math functions:

```text
ABS INT ROUND MIN MAX SQRT SIN COS TAN
ASIN ACOS ATAN ATAN2 LOG EXP RND DEG RAD
```

Constants: `PI`, `E`.

Example program:

```basic
10 REM SUM 1 TO 5
20 LET S = 0
30 FOR I = 1 TO 5
40 S = S + I
50 NEXT I
60 PRINT S
70 END
```

## Current next step

Phase 1 shell, BASIC storage, and reboot persistence are hardware-tested. The
next implementation phase is native RISC-V assembly support.
