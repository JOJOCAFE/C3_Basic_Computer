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
  firmware exposes terminal hardware clients through `/bin/hardware`; BASIC
  hardware access is deferred to a direct `source/hardware` API layer.
- Native RISC-V assembly capture is the next candidate milestone. Native
  execution remains blocked until a later guarded runtime sprint.
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
| `RENEW` | Works | Requires two confirmations, formats only `workspace_fs`, and rebuilds the workspace layout. |

Checked with:

```bash
python3 tools/workspace_shell_smoke.py --port /dev/ttyACM0
python3 tools/no_basic_shell_smoke.py --port /dev/ttyACM0
python3 tools/renew_full_smoke.py --port /dev/ttyACM0
python3 tools/adversarial_shell_smoke.py --port /dev/ttyACM0
```

Not exposed by the boot shell: `DIR`, `COPY`, `MOVE`, `RENAME`, `DELETE`,
`LOAD`, `SAVE`, `NEW`, `LIST`, `RUN`, and direct BASIC statements such as
`PRINT`.

Later workspace/system/monitor targets:
`BASIC`, `ASM`, `VERSION`, `MEMORY`, `DATE`, `TIME`, `DIAGNOSTICS`,
`REG`, `MEM`, `DUMP`, `DISASM`, `STEP`, `BREAK`

`EDIT <path>` is the text editor command backed by `/bin/nano`.
Future `BASIC <path>` and `ASM <path>` commands should dispatch to the same
editor service with the BASIC or ASM plugin selected.
Linked services can be listed with `/bin list`.

Only implemented commands should appear in firmware `HELP`.

## Board-checked nano editor commands

Nano is a line-oriented `.txt` editor service. It works through either the shell
alias or the canonical `/bin` service path:

```text
EDIT /data/note.txt
/bin/nano /data/note.txt
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
:help      Help
```

Current limits and behavior:

- Edits `/data/*.txt` only.
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

## Implementation plan (recommended next steps)

Sprint 002 shell-first work is complete. Resume in the order below.

1. **Completed - micro Linux workspace shell**
   - `HELP`, `PWD`, `CD`, `LS`, `MKDIR`, `RMDIR`, `CAT`, `WRITE`, `RM`,
     `RM -R`, `CP`, `MV`, and `RENEW` are implemented.
   - Keep all file commands constrained to `/workspace`.
   - Keep BASIC commands out of the boot shell until a separate BASIC runtime
     entry point is designed.
   - Keep `FORMAT`, `BOOT`, `RAMBOOT`, `XIP`, `PXE`, and OTA out of this sprint.

2. **Completed – Input boundary**
   - PC terminal over USB Serial/JTAG is the active backend.
   - Shell input is behind an input service.
   - BLE HID keyboard backend boundary exists; real pairing waits for a keyboard.

3. **In progress - nano text editor service**
   - Lean nano-style `/bin/nano` editor service with `EDIT <path>` as the shell
     command, documented in
   [`docs/SPRINT_004_NANO_EDITOR_SERVICE.md`](docs/SPRINT_004_NANO_EDITOR_SERVICE.md).
   - First implementation target is `/data/*.txt` only.
   - Later, `BASIC <path>` and `ASM <path>` should call nano with removable
     language plugins/services so the boot shell stays small.

4. **Next candidate - `/bin` service ABI and pipes**
   - Current `/bin` services use a registry and build-time selection.
   - Stream ABI and `|` pipes are planned in
     [`docs/SPRINT_005_BIN_SERVICE_ABI_AND_PIPES.md`](docs/SPRINT_005_BIN_SERVICE_ABI_AND_PIPES.md).

5. **Later - ASM capture boundary**
   - Parse and capture `ASM`/`ENDASM` blocks from BASIC safely.
   - Keep assembly source separate from BASIC source storage.
   - Validate assembler input before any execution work.

6. **Later - Runtime, monitor, and capability expansion**
   - Add `CALLASM()`, standalone `.asm` execution, and monitor commands only after
     capture/validation passes.
   - Add graphics, sound, BASIC GPIO/motion, then standalone UX hardware
     integrations as separate milestones.

## UX and behavior goals

- `READY.` is the standard idle state.
- Errors explain what happened and where it happened; avoid numeric codes.
- Warnings are reserved for potentially destructive actions.
- Success returns to `READY.` with minimal noise.
- Keep the system small, understandable, and transparent.

## Compatibility intent

Architecture and behavior are designed to stay consistent across the C-Series
BASIC COMPUTER family where practical (C3/C5/C6/P4).
