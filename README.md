# C3 BASIC COMPUTER

Educational, terminal-first computer system for the ESP32-C3 with a focus on
maker-friendly programming, recoverability, and incremental implementation.

This project is documented as a reference implementation.  
Core ideas:
- Computer-first architecture (not firmware-first)
- Simple, predictable shell-driven workflow
- Tiny text-based system designed for learning and hacking

Current implementation context:
- Phase 1 is now oriented around recoverability.
- Flash partitioning is split into two LittleFS areas:
  - `system_fs` (read-mostly runtime boundary)
  - `workspace_fs` (renewable user workspace)
- Boot and protected recovery do not depend on workspace files. If `workspace_fs`
  is damaged, the shell still starts from the protected path and `RENEW` can
  rebuild workspace after two confirmations.
- Sprint 002 micro Linux workspace shell is complete and board-tested,
  using the local OpenC6 BIOS fork as a structure reference.
- Hardware service package is complete for GPIO/ADC/I2C/SPI C APIs. The root
  firmware exposes terminal hardware clients through `/bin/hardware`; BASIC now
  has a typed GPIO/ADC adapter over `source/hardware` for the first slice.
- Shell YMODEM transfer and guarded C3COM execution are board-tested. Native
  uploaded `.com` programs can run only through explicit `RUN /bin/name.com`
  after C3COM header and CRC validation.
- BLE HID keyboard support has a compiled input boundary, but real keyboard
  pairing remains pending until a keyboard is available.

## Design snapshot

- Boot experience: `READY.` prompt after startup
- Workspace path: `/` with writable folders (`/basic`, `/asm`, `/bin`, `/config`,
  `/data`, `/temp`)
- Renewed workspace is constrained to `workspace_fs`; system storage remains separate.
- `RENEW` is protected behavior and formats only `workspace_fs`; there is no
  public `FORMAT` command in the shell sprint.
- Command style: micro Linux workspace shell commands, with `RENEW` as the
  protected recovery exception.
- BASIC and assembly remain main maker interfaces, but they are no longer
  exposed directly from the boot shell.
- Recoverability: system-protected runtime plus renewable user workspace

## Repository map

- `docs/` — architecture, language, shell, filesystem, commands, and roadmap
- `main/` — ESP-IDF application component and shared firmware services
- `source/shell/` — micro Linux boot shell source, command API, command list, and test matrix
- `source/hardware/` — reusable GPIO, ADC, I2C, and SPI C service APIs
- `source/bin/` — root-firmware `/bin` service adapters, including `/bin/hardware`
- `tools/` — current build scripts and serial smoke tests
- `hardware/` — board-level hardware notes/design artifacts
- `assets/`, `examples/`, `test/` — project assets, samples, and checks
- `.codex/PROJECT_SKILL.md` — project skill contract (Manifesto + Design Principles)

## Build and flash

The stable launcher is in `tools/idf53.sh` and expects ESP-IDF 5.3.x:

```bash
tools/idf53.sh -B build-c3-root build
tools/idf53.sh -B build-c3-root -p /dev/ttyACM0 flash
```

Common host-side checks:

```bash
python3 tools/workspace_shell_smoke.py --port /dev/ttyACM0
python3 tools/no_basic_shell_smoke.py --port /dev/ttyACM0
python3 tools/renew_full_smoke.py --port /dev/ttyACM0
python3 tools/adversarial_shell_smoke.py --port /dev/ttyACM0
python3 tools/nano_editor_smoke.py --port /dev/ttyACM0 --timeout 25
python3 tools/bin_pipe_smoke.py --port /dev/ttyACM0
python3 tools/bin_hardware_gpio_smoke.py --port /dev/ttyACM0 --pin 8 --seconds 10
python3 tools/bin_hardware_adc_smoke.py --port /dev/ttyACM0 --pin 0
python3 tools/bin_hardware_i2c_smoke.py --port /dev/ttyACM0 --sda 6 --scl 7
python3 tools/bin_hardware_spi_smoke.py --port /dev/ttyACM0 --mosi 4 --miso 5 --sclk 6 --cs 7
python3 tools/ymodem_transfer_smoke.py --port /dev/ttyACM0
python3 tools/c3com_fixture_smoke.py --smoke --port /dev/ttyACM0
```

## Board-checked shell commands

The boot shell is a micro Linux workspace shell. These commands are implemented
in firmware and have been checked on the ESP32-C3 board over `/dev/ttyACM0`.

| Command | Status | Behavior checked |
| --- | --- | --- |
| `HELP` | Works | Prints only the implemented shell commands. |
| `PWD` | Works | Prints the current workspace-relative directory. |
| `LS [path]` | Works | Lists workspace entries; directories are shown as `/name`, files as `name`. |
| `CD <path>` | Works | Changes current workspace directory. |
| `MKDIR <path>` | Works | Creates directories inside `/workspace`. |
| `RMDIR <dir>` | Works | Removes an empty directory inside `/workspace`. |
| `CAT <file>` | Works | Prints a text file. |
| `WRITE <file> <text>` | Works | Creates a text file inside `/workspace`. |
| `RM <file>` | Works | Removes files; directories are rejected unless `-R` is used. |
| `RM -R <path>` | Works | Recursively removes a workspace file or directory subtree. |
| `CP <src> <dst>` | Works | Copies files inside `/workspace`. |
| `MV <src> <dst>` | Works | Moves or renames files and directories inside `/workspace`. |
| `DF` | Works | Shows workspace total, used, and available 1K blocks. |
| `RECV [-F] <path>` | Works | Receives one YMODEM file into the exact workspace path, with overwrite guard and free-space preflight. |
| `SEND <path>` | Works | Sends one workspace file by YMODEM with exact size and bytes. |
| `RUN /bin/name.com [args...]` | Works | Runs guarded C3COM programs with argv, standard I/O callbacks, CRC validation, and `EXIT n` reporting. |
| `RENEW` | Works | Requires two confirmations, formats only `workspace_fs`, and rebuilds the workspace layout. |

Checked with:

```bash
python3 tools/workspace_shell_smoke.py --port /dev/ttyACM0
python3 tools/no_basic_shell_smoke.py --port /dev/ttyACM0
python3 tools/renew_full_smoke.py --port /dev/ttyACM0
python3 tools/adversarial_shell_smoke.py --port /dev/ttyACM0
python3 tools/ymodem_transfer_smoke.py --port /dev/ttyACM0
python3 tools/c3com_fixture_smoke.py --smoke --port /dev/ttyACM0
```

Not exposed by the boot shell: `DIR`, `COPY`, `MOVE`, `RENAME`, `DELETE`,
`LOAD`, `SAVE`, `NEW`, `LIST`, and direct BASIC statements such as `PRINT`.
Bare `RUN` is a guarded C3COM command and prints usage; it does not enter BASIC
immediate mode.

Later workspace/system/monitor targets:
`ASM`, `VERSION`, `MEMORY`, `DATE`, `TIME`, `DIAGNOSTICS`, `REG`, `MEM`,
`DUMP`, `DISASM`, `STEP`, `BREAK`

`EDIT <path>` is the text editor command backed by `/bin/nano`.
`BASIC <path>` dispatches to the same editor service with the BASIC plugin
selected. Future `ASM <path>` should dispatch to nano with the ASM plugin
selected.
Linked services can be listed with `/bin list`.

Only implemented commands should appear in firmware `HELP`.

## Board-checked nano editor commands

Nano is a line-oriented `.txt` editor service. It works through either the shell
alias or the canonical `/bin` service path:

```text
EDIT /data/note.txt
/bin/nano /data/note.txt
```

BASIC source opens through the same nano editor service in BASIC mode:

```text
BASIC /basic/hello.bas
```

Current BASIC editor mode accepts `/basic/*.bas`, validates numbered BASIC lines
before save, and keeps the runtime separate from the boot shell. `:run` saves
and runs the open BASIC buffer from nano with the 64 KiB editor-run loader.
`:debug` step-runs the BASIC program one statement at a time.

The first BASIC runtime is intentionally tiny: numbered lines, integer
expressions, single-letter variables, `REM`, `PRINT`, `LET`/assignment,
`INPUT`, `IF THEN [ELSE]`, `GOTO`, `GOSUB`/`RETURN`, `FOR`/`NEXT`, `END`/`STOP`,
`PINMODE`, `DWRITE`, `DTOGGLE`, `DREAD()`, `AREAD()`, and `ABS`, `INT`, `RND`.
Extended graphics, sound, network, arrays, strings, I2C/SPI BASIC commands, and
modern unnumbered BASIC are deferred.

BASIC service calls are limited to explicitly whitelisted safe operations:
`SHELL "PWD"`, `SHELL "CAT <file>"`, `HARDWARE "gpio read -p <pin>"`, and
`HARDWARE "adc read -p <pin>"`. Directory listing from BASIC is deferred until
it has a stack-safe adapter. Destructive commands such as `RENEW`, `RM`,
`WRITE`, `CP`, `MV`, and native execution stay blocked.

Preferred BASIC hardware access is typed and calls `source/hardware` directly:

```basic
10 PINMODE 8, OUTPUT
20 DWRITE 8, HIGH
30 PRINT DREAD(8)
40 PRINT AREAD(0)
50 END
```

Inside nano:

```text
Text line  Append text
:w         Save
:q         Quit if clean
:q!        Quit without saving
:wq        Save and quit
:p         Print buffer
:clear     Clear buffer
:run       Save and run BASIC program in BASIC mode
:debug     Save and step-run BASIC program in BASIC mode
:help      Help
```

Current limits and behavior:

- `EDIT` edits `/data/*.txt` only.
- `BASIC` edits `/basic/*.bas` only.
- Text buffer is 64 KiB per open file, including line separators.
- Input line buffer is 64 KiB, bounded by the same editor capacity.
- Allocation failure prints `Out of memory` and returns to the shell.
- Opening a file larger than the editor buffer prints `File too large`.
- Appending beyond the editor buffer prints `Buffer full` and returns to the shell.
- Thai text and Thai filenames under `/data` have been board-tested.

## Board-checked hardware commands

Hardware terminal commands are `/bin` services above the shell. They are not
shell built-ins and do not appear in `HELP`.

Current hardware services:

```text
/bin/hardware gpio in -p <gpio> [--pull none|up|down|updown]
/bin/hardware gpio out -p <gpio> [-v 0|1] [--open-drain]
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

The current ESP32-C3 board blue LED is on GPIO8 and turns off at level `1`:

```text
/bin/hardware gpio out -p 8 -v 1
/bin/hardware gpio write -p 8 -v 1
```

Detailed hardware docs:

- [`source/hardware/README.md`](/home/jo/Codex/C3_Basic_Computer/source/hardware/README.md)
- [`source/hardware/COMMANDS.md`](/home/jo/Codex/C3_Basic_Computer/source/hardware/COMMANDS.md)
- [`source/hardware/VERIFICATION.md`](/home/jo/Codex/C3_Basic_Computer/source/hardware/VERIFICATION.md)
- [`docs/SPRINT_003_BASIC_HARDWARE_API.md`](/home/jo/Codex/C3_Basic_Computer/docs/SPRINT_003_BASIC_HARDWARE_API.md)

## Strict implementation checklist

See [`docs/IMPLEMENTATION_CHECKLIST.md`](/home/jo/Codex/C3_Basic_Computer/docs/IMPLEMENTATION_CHECKLIST.md) for a folder/file-level execution plan and acceptance criteria.

Completed high-priority implementation plan:

[`docs/SPRINT_002_TASK_LIST.md`](/home/jo/Codex/C3_Basic_Computer/docs/SPRINT_002_TASK_LIST.md)

## Current implementation status

Completed and board-checked:

1. **Micro Linux workspace shell**
   - `HELP`, `PWD`, `LS`, `CD`, `MKDIR`, `RMDIR`, `CAT`, `WRITE`, `RM`,
     `RM -R`, `CP`, `MV`, and protected `RENEW`.
   - File commands stay constrained to `/workspace`.
   - BASIC immediate statements remain outside the boot shell.

2. **Input boundary**
   - USB Serial/JTAG is the active tested backend.
   - Shell input is behind an input service.
   - BLE HID keyboard backend code exists; real pairing still needs hardware.

3. **Nano editor and BASIC mode**
   - `EDIT /data/name.txt` and `/bin/nano /data/name.txt` edit text.
   - `BASIC /basic/name.bas` opens nano in BASIC mode.
   - `:run` and `:debug` run or step the current numbered BASIC buffer.
   - Editor and BASIC source buffers use the 64 KiB nano buffer.

4. **BASIC runtime first slice**
   - Numbered-line tiny BASIC with integer expressions, single-letter variables,
     core control flow, and structured validation errors.
   - Safe BASIC service calls: `SHELL "PWD"`, `SHELL "CAT <file>"`,
     `HARDWARE "gpio read -p <pin>"`, and `HARDWARE "adc read -p <pin>"`.
   - Typed BASIC GPIO/ADC commands call `source/hardware` directly.

5. **Hardware services**
   - `source/hardware` provides GPIO/ADC/I2C/SPI C APIs.
   - `/bin/hardware` exposes terminal GPIO/ADC/I2C/SPI clients.
   - BASIC has a typed GPIO/ADC adapter over the same service layer.

6. **`/bin` registry and RAM pipes**
   - `/bin list`, `/bin/nano`, `/bin/hardware`, and a first RAM-backed pipe path
     are implemented.

7. **Shell YMODEM transfer and guarded C3COM execution**
   - `DF`, `RECV`, and `SEND` move `.bas`, `.asm`, `.com`, config, and test
     files over the terminal without reflashing.
   - `RUN /bin/name.com [args...]` validates C3COM headers and CRC before
     copying code to executable RAM and jumping.
   - Native C3COM execution requires ESP-IDF memory protection disabled on
     ESP32-C3 so executable heap is available.
   - First-slice `.com` programs must be position-independent flat code because
     there is no relocation loader yet.

Recommended next implementation:

1. Start Sprint 010: [`docs/SPRINT_010_TERMINAL_SERVICE_AND_BASIC_TERM.md`](docs/SPRINT_010_TERMINAL_SERVICE_AND_BASIC_TERM.md).
2. Add `/bin/term` as an output-only ANSI/VT100 terminal service.
3. Add BASIC `TERM "..."` as a safe bridge to `/bin/term` for simple text UI.
4. Then resume Sprint 009: [`docs/SPRINT_009_ASM_NANO_MODE_TASK_LIST.md`](docs/SPRINT_009_ASM_NANO_MODE_TASK_LIST.md).
5. Add ASM nano mode: `ASM /asm/name.asm` should edit and validate text only.
6. Resume ASM capture as a non-execution milestone after ASM editor mode is
   stable and board-tested.
7. Add a future C3COM relocation/toolchain slice if normal C output should use
   `.rodata` and relocations.
8. Add system/monitor commands only after their behavior is documented and
   testable.

## UX and behavior goals

- `READY.` is the standard idle state.
- Errors explain what happened and where it happened; avoid numeric codes.
- Warnings are reserved for potentially destructive actions.
- Success returns to `READY.` with minimal noise.
- Keep the system small, understandable, and transparent.

## Compatibility intent

Architecture and behavior are designed to stay consistent across the C-Series
BASIC COMPUTER family where practical (C3/C5/C6/P4).
