# C3 BASIC COMPUTER Compliance Matrix

Document source: `docs/`  
Implementation snapshot: `Old_version/main/*`

## Legend

- `PASS`: behavior currently exists and matches the documented intent
- `PARTIAL`: partially implemented or mismatched by detail
- `FAIL`: documented behavior exists but is not currently implemented
- `NOT_APPLICABLE`: foundational or future-only requirement not represented in current phase

## 1) Core experience

| Requirement | Source | Status | Current Evidence | Gap / Action |
| --- | --- | --- | --- | --- |
| Computer-first design (architecture, recoverability, transparency) | `docs/01_Design_Principles.md`, `docs/02_Software_Architecture.md`, `docs/03_User_Experience_Guide.md` | PASS | `Old_version/main` boots directly into shell and presents command/programming flow. | No explicit design guardrails in runtime checks; still acceptable for phase 1. |
| Boot-to-READY flow and simple shell prompt | `docs/03_User_Experience_Guide.md`, `docs/04_Shell_Reference.md` | PASS | `Old_version/main/shell.c` prints `C3 BASIC COMPUTER`, `Version 0.1`, `READY.`. | Keep stable. |
| Friendly, non-numeric error style | `docs/03_User_Experience_Guide.md`, `docs/04_Shell_Reference.md` | PARTIAL | shell prints text messages, but many BASIC parse failures collapse to `BASIC ERROR` or `UNKNOWN COMMAND`. | Add line-aware and field-specific messages in remaining paths. |
| Recoverability via renewable workspace | `docs/05_File_System.md` | PARTIAL | `RENEW` command is implemented with confirmation, but command help and command list are still phased in. | Confirmed behavior and destructive confirmation are implemented; finish cross-doc test evidence. |
| READY after operations | `docs/03_User_Experience_Guide.md` | PARTIAL | prompt loop exists, but not every path is verified to return READY semantics consistently. | Normalize end-of-command transitions and warnings. |

## 2) Layer coverage vs architecture

| Requirement | Source | Status | Current Evidence | Gap / Action |
| --- | --- | --- | --- | --- |
| Shell command groups (core/workspace/system/monitor) | `docs/04_Shell_Reference.md`, `docs/10_System_Commands.md` | PARTIAL | Core+some workspace+no system/monitor commands implemented. | Implement at least `version`, `memory`, `date`, `time`, `update`, `diagnostics`, and monitor set. |
| Storage services (LittleFS workspace + directory layout) | `docs/05_File_System.md`, `docs/02_Software_Architecture.md` | PARTIAL | `Old_version/main/storage.c` mounts two LittleFS partitions and creates `/basic,/asm,/bin,/config,/data,/temp` (+ legacy uppercase aliases). | Add system/ workspace usage observability + legacy-to-canonical migration expectations. |
| Workspace architecture `/basic`, `/asm`, `/config`, `/data`, `/temp` | `docs/05_File_System.md` | PARTIAL | `storage_resolve_path` uses canonical lowercase workspace layout, with uppercase compatibility aliases in runtime defaults. | Align remaining command docs and legacy migration behavior. |
| Editor / Workspace features (`edit`, `run`, `asm`) | `docs/02_Software_Architecture.md`, `docs/08_Workspace.md` | FAIL | only shell-only flow; no dedicated editor workspace actions. | Add `edit` and workspace action semantics or document as phased-out only if temporary. |
| System services (version/time/memory/diagnostics) | `docs/02_Software_Architecture.md`, `docs/10_System_Commands.md` | FAIL | Not implemented in current shell. | Implement command set in phase 3+. |

## 3) BASIC language and library

| Requirement | Source | Status | Current Evidence | Gap / Action |
| --- | --- | --- | --- | --- |
| BASIC command parsing (REM/LET/PRINT/INPUT/IF/FOR/GOTO/GOSUB/etc.) | `docs/06_BASIC_Language.md`, `docs/07_BASIC_Library.md` | PASS | `Old_version/main/basic.c` supports these plus arithmetic and conditionals. | Keep behavior and expand to modern syntax coverage. |
| Case-sensitive BASIC tokens | `docs/06_BASIC_Language.md` | PASS | parser generally normalizes via uppercase and accepts case-insensitive shell keywords only. | shell command matching is case-insensitive by design for user convenience; acceptable only if consciously accepted as compatibility extension. |
| Program text format support (`classic` + `modern`) | `docs/06_BASIC_Language.md` | PARTIAL | both numbered and unnumbered are handled via `RUN`/`basic_execute_immediate`, but semantics are tighter than documented. | Improve parser acceptance for modern unnumbered flow and line conversions. |
| Built-in math/functions constants | `docs/06_BASIC_Language.md`, `docs/07_BASIC_Library.md` | PASS | math functions/constants are present including PI/E and many trig/log functions. | Keep numeric semantics stable (currently `double -> int` rounding paths). |
| BASIC library modules (Graphics/Sound/Motion/GPIO/Network) | `docs/07_BASIC_Library.md` | FAIL | Not implemented. | Deferred to later milestones; update docs to mark staged availability if required. |

## 4) Native RISC-V assembly milestone

| Requirement | Source | Status | Current Evidence | Gap / Action |
| --- | --- | --- | --- | --- |
| `ASM/ENDASM`, `CALLASM`, assembler, execution | `docs/09_Native_RISC-V_Assembly.md`, `docs/IMPLEMENTATION_PLAN.md` | FAIL | Not yet in production code path. | Implement phase 2A/2B as guarded milestones. |
| Assembly tools (`asm`, `disasm`, `reg`, `mem`, `dump`, `step`, `break`) | `docs/04_Shell_Reference.md`, `docs/09_Native_RISC-V_Assembly.md` | FAIL | None are implemented. | Add in monitored, safe order. |

## 5) I/O and drivers

| Requirement | Source | Status | Current Evidence | Gap / Action |
| --- | --- | --- | --- | --- |
| USB-serial input pipeline phase 1 | `docs/12_Input_and_Drivers.md` | PASS | `usb_serial_jtag` used as shell transport. | Confirm reliability edge cases and key handling consistency. |
| Keyboard input event model / key mapping | `docs/12_Input_and_Drivers.md` | NOT_APPLICABLE | No explicit key-event abstraction layer. | Add driver abstraction for future phase-2+ input paths. |
| BLE keyboard and standalone hardware drivers | `docs/12_Input_and_Drivers.md`, `docs/IMPLEMENTATION_PLAN.md` | NOT_APPLICABLE (Phase 2+) | Not in scope for current firmware branch. | Track as next hardware milestones. |

## 6) Documentation vs behavior drift

| Requirement | Source | Status | Current Evidence | Gap / Action |
| --- | --- | --- | --- | --- |
| Command surface documented as current milestone | `docs/IMPLEMENTATION_PLAN.md`, `docs/04_Shell_Reference.md` | FAIL | `Old_version/README.md` claims subset (`DIR/LOAD/SAVE...`), while docs now list broader shell/sys/monitor sets. | Add explicit versioned behavior matrix in README and milestone gate checklist. |
| Demonstrable milestone criteria | `docs/IMPLEMENTATION_PLAN.md` | PARTIAL | smoke scripts exist in `Old_version/tools`, but no direct doc-driven evidence for all required outputs. | Convert each milestone to executable smoke + output assertions. |

## Priority remediation plan (next 2 sprints)

1. Align storage namespace and workspace layout (`/basic,/asm,/bin,/config,/data,/temp`) without breaking compatibility.
2. Add shell command set baseline: `version`, `memory`, `date`, `time`, `renew`, `diagnostics`.
3. Add `help <command>` discovery behavior and keep unknown-command hard-fail stable.
4. Add `asm` capture and validation boundary first (phase 2A) before enabling execution.
5. Implement monitor command stubs or explicit not-supported messages with predictable outputs.

## Notes

- This matrix is a planning artifact, not an official release claim.
- Scope policy for this cycle: `Old_version` is a working reference implementation for the current device state, not a full-document compliance target.
