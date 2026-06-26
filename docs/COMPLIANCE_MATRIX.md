# C3 BASIC COMPUTER Compliance Matrix

Document source: `docs/`  
Implementation snapshot: root ESP-IDF project under `main/`, shell feature under
`source/shell/`, hardware services under `source/hardware` and `source/bin`,
and smoke tooling under `tools/`.

## Legend

- `PASS`: behavior currently exists and matches the documented intent
- `PARTIAL`: partially implemented or mismatched by detail
- `FAIL`: documented behavior exists but is not currently implemented
- `NOT_APPLICABLE`: foundational or future-only requirement not represented in current phase

## 1) Core experience

| Requirement | Source | Status | Current Evidence | Gap / Action |
| --- | --- | --- | --- | --- |
| Computer-first design (architecture, recoverability, transparency) | `docs/01_Design_Principles.md`, `docs/02_Software_Architecture.md`, `docs/03_User_Experience_Guide.md` | PASS | `main/` boots directly into shell and presents command/programming flow. | No explicit design guardrails in runtime checks; still acceptable for phase 1. |
| Boot-to-READY flow and simple shell prompt | `docs/03_User_Experience_Guide.md`, `docs/04_Shell_Reference.md` | PASS | `source/shell/shell.c` prints `C3 BASIC COMPUTER`, `Version 0.1`, `READY.`. | Keep stable. |
| Friendly, non-numeric error style | `docs/03_User_Experience_Guide.md`, `docs/04_Shell_Reference.md` | PARTIAL | shell prints short text messages and rejects unsupported commands with `UNKNOWN COMMAND`. | Add field-specific messages where useful. |
| Recoverability via renewable workspace | `docs/05_File_System.md` | PASS | `RENEW` command is implemented with two confirmations, formats only `workspace_fs`, and remains available from the protected shell path. Full destructive renew smoke passed on 2026-06-26. | Future damaged-workspace corruption simulation can broaden coverage. |
| READY after operations | `docs/03_User_Experience_Guide.md` | PARTIAL | prompt loop exists, but not every path is verified to return READY semantics consistently. | Normalize end-of-command transitions and warnings. |

## 2) Layer coverage vs architecture

| Requirement | Source | Status | Current Evidence | Gap / Action |
| --- | --- | --- | --- | --- |
| Shell command groups (core/workspace/system/monitor) | `docs/04_Shell_Reference.md`, `docs/10_System_Commands.md` | PARTIAL | Sprint 002 core/workspace commands are implemented: `HELP`, `PWD`, `LS`, `CD`, `MKDIR`, `RMDIR`, `CAT`, `WRITE`, `RM`, `RM -R`, `CP`, `MV`, and protected `RENEW`. BASIC is deferred out of the boot shell. | System information, monitor, and BASIC runtime entry points remain future milestones. |
| Storage services (LittleFS workspace + directory layout) | `docs/05_File_System.md`, `docs/02_Software_Architecture.md` | PASS | `main/storage.c` mounts split LittleFS partitions, creates `/basic,/asm,/bin,/config,/data,/temp`, and keeps public file commands under `/workspace`. | Add optional workspace usage observability later. |
| Workspace architecture `/basic`, `/asm`, `/config`, `/data`, `/temp` | `docs/05_File_System.md` | PARTIAL | `storage_resolve_path` uses canonical lowercase workspace layout, with uppercase compatibility aliases in runtime defaults. | Align remaining command docs and legacy migration behavior. |
| Editor / Workspace features (`edit`, `run`, `asm`) | `docs/02_Software_Architecture.md`, `docs/08_Workspace.md` | FAIL | only shell-only flow; no dedicated editor workspace actions. | Add `edit` and workspace action semantics or document as phased-out only if temporary. |
| System services (version/time/memory/diagnostics) | `docs/02_Software_Architecture.md`, `docs/10_System_Commands.md` | FAIL | Not implemented in current shell. | Implement command set in phase 3+. |

## 3) BASIC language and library

| Requirement | Source | Status | Current Evidence | Gap / Action |
| --- | --- | --- | --- | --- |
| BASIC command parsing (REM/LET/PRINT/INPUT/IF/FOR/GOTO/GOSUB/etc.) | `docs/06_BASIC_Language.md`, `docs/07_BASIC_Library.md` | PASS | `main/basic.c` supports these plus arithmetic and conditionals. | Keep behavior and expand to modern syntax coverage. |
| Case-sensitive BASIC tokens | `docs/06_BASIC_Language.md` | PASS | parser generally normalizes via uppercase and accepts case-insensitive shell keywords only. | shell command matching is case-insensitive by design for user convenience; acceptable only if consciously accepted as compatibility extension. |
| Program text format support (`classic` + `modern`) | `docs/06_BASIC_Language.md` | PARTIAL | parser support exists in `basic.c`, but no boot-shell command currently exposes BASIC editing or execution. | Design a separate BASIC runtime/editor entry point. |
| Built-in math/functions constants | `docs/06_BASIC_Language.md`, `docs/07_BASIC_Library.md` | PASS | math functions/constants are present including PI/E and many trig/log functions. | Keep numeric semantics stable (currently `double -> int` rounding paths). |
| BASIC library modules (Graphics/Sound/Motion/GPIO/Network) | `docs/07_BASIC_Library.md` | FAIL | BASIC-facing library commands are not implemented. The underlying GPIO/ADC/I2C/SPI C service package now exists separately in `source/hardware`. | Deferred to later milestones; BASIC GPIO should call `source/hardware` when implemented. |

## 4) Native RISC-V assembly milestone

| Requirement | Source | Status | Current Evidence | Gap / Action |
| --- | --- | --- | --- | --- |
| `ASM/ENDASM`, `CALLASM`, assembler, execution | `docs/09_Native_RISC-V_Assembly.md`, `docs/IMPLEMENTATION_PLAN.md` | FAIL | Not yet in production code path. | Implement phase 2A/2B as guarded milestones. |
| Assembly tools (`asm`, `disasm`, `reg`, `mem`, `dump`, `step`, `break`) | `docs/04_Shell_Reference.md`, `docs/09_Native_RISC-V_Assembly.md` | FAIL | None are implemented. | Add in monitored, safe order. |

## 5) I/O and drivers

| Requirement | Source | Status | Current Evidence | Gap / Action |
| --- | --- | --- | --- | --- |
| USB-serial input pipeline phase 1 | `docs/12_Input_and_Drivers.md` | PASS | `input_serial.c` owns the USB Serial/JTAG line editor behind the input API, and final shell/BASIC/adversarial smoke tests passed. | Keep as recovery/development backend. |
| Keyboard input event model / key mapping | `docs/12_Input_and_Drivers.md` | PARTIAL | `input.h` defines the input event boundary and `input_ble_hid.c` includes a boot-keyboard ASCII mapper. | Complete full key queue, modifier, arrow, and reconnect behavior when BLE host integration is enabled. |
| BLE keyboard input backend | `docs/12_Input_and_Drivers.md`, `docs/IMPLEMENTATION_PLAN.md` | PARTIAL | BLE HID backend boundary is compiled but inactive; keyboard pairing is not tested because the real keyboard is not available yet. | Enable ESP-IDF HID host, pair real keyboard, and verify disconnect/reconnect when a keyboard is available. |
| Board hardware service boundary | `docs/02_Software_Architecture.md`, `docs/12_Input_and_Drivers.md`, `source/hardware/README.md` | PASS | `source/hardware` provides GPIO/ADC/I2C/SPI C APIs; root firmware registers `/bin/hardware` GPIO/ADC/I2C/SPI adapters; GPIO8 board test passed through the shell. | Run board smoke for the newly wired ADC/I2C/SPI adapters. |

## 6) Documentation vs behavior drift

| Requirement | Source | Status | Current Evidence | Gap / Action |
| --- | --- | --- | --- | --- |
| Command surface documented as current milestone | `docs/IMPLEMENTATION_PLAN.md`, `docs/04_Shell_Reference.md` | PASS | README, shell reference, and Sprint 002 task list now document the implemented shell command set and defer system/monitor commands. | Keep firmware `HELP` and docs synchronized for future commands. |
| Demonstrable milestone criteria | `docs/IMPLEMENTATION_PLAN.md` | PASS | Build, flash, workspace shell, renew, no-BASIC-shell, adversarial, hardware GPIO smoke, and ADC/I2C/SPI adapter build evidence are maintained for the current boundaries. | Add board-smoke evidence for ADC/I2C/SPI, then keep the same discipline for later BASIC, ASM, and BLE milestones. |

## Priority remediation plan (next 2 sprints)

1. Resume ASM capture as a non-execution milestone.
2. Keep native execution blocked until assembler validation and runtime guardrails exist.
3. Preserve the dual-partition storage boundary and keep all public file commands inside `/workspace`.
4. Pair and test the BLE HID keyboard backend when a real keyboard is available.
5. Add system information and monitor commands only after their behavior is documented and testable.

## Notes

- This matrix is a planning artifact, not an official release claim.
- Scope policy for this cycle: `Old_version` is legacy/reference history only; active implementation and tests live at the repository root.
