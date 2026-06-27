# C3 BASIC COMPUTER Project Skill

This file is the persistent project skill record for the repository.

## Manifesto

Play.
Hack.
Build.
Share.
Stay Curious.
Keep Going.

## Design Principles

1. Computer First  
   C3 BASIC COMPUTER is a computer. The microcontroller is an implementation detail.

2. Instant On  
   Power on. READY. Start creating.

3. Programming First  
   Programming is the primary interface.

4. Playable  
   The system should invite experimentation. The first program should take minutes, not hours.

5. Hackable  
   Every practical layer should be open to exploration.

6. Recoverable  
   Exploration should never permanently damage the computer.  
   The system software is protected. The user workspace is renewable.

7. Transparent  
   The system should reveal how it works.

8. Companion  
   The computer communicates clearly. It encourages exploration and never gets in the maker's way.

9. Honor Computing Traditions  
   Preserve timeless ideas when they continue to help understanding.  
   READY. HELLO, WORLD! ERROR. HELP.

10. Build for Makers  
    Designed for people who build, modify, experiment, and share.

11. Reference, Not Restriction  
    This project provides a reference implementation. Others are encouraged to modify, extend, and experiment.

12. Keep It Small  
    Prefer simple systems over complicated ones.  
    A feature is complete when it is understandable.

## Use Rule

When implementing or planning, treat this file as the authoritative behavior contract:

- Keep changes aligned with the manifesto and design principles.
- Prefer incremental milestones.
- Preserve recoverability and simple user-facing behavior.
- Do not remove core shell/BASIC foundations without a replacement.
- Every source-code file must end with the exact line comment `//Keep Going.`
  when the file format supports `//` comments.

## Current Sprint Routing

- Sprint 002 micro Linux workspace shell is complete and board-tested.
- Use the local OpenC6 BIOS fork as a shell-structure reference only:
  - current-directory state
  - path resolver
  - command loop
  - small file command handlers
- Do not import OpenC6 `format`, `boot ram`, `boot xip`, XIP, PXE, OTA, UART1-only routing, or payload ABI behavior into the C3 shell sprint.
- Keep the boot shell as a micro Linux workspace shell only: `HELP`, `DF`, `PWD`,
  `LS`, `CD`, `MKDIR`, `RMDIR`, `CAT`, `WRITE`, `RM`, `RM -R`, `CP`, `MV`,
  `RECV`, `SEND`, and protected `RENEW`.
- Shell file transfer is a protected core feature, not a removable `/bin`
  service. Use YMODEM `RECV` / `SEND` to move `.bas`, `.asm`, `.com`, config,
  and test files over the terminal without reflashing. `.com` transfer does not
  imply `.com` execution.
- Shell `RUN /bin/name.com [args...]` is guarded C3COM execution only. It must
  validate the C3COM header before jumping, pass argv plus standard I/O through
  the ABI, and never auto-run after transfer.
- Active shell source and shell feature docs live in `source/shell/`.
- Active build/test tooling lives in root `tools/`; root source is the active build target.
- BASIC parsing code remains in the tree for a later runtime/editor entry point,
  but BASIC commands are not exposed by the boot shell.
- Editor planning now routes through `docs/SPRINT_004_NANO_EDITOR_SERVICE.md`:
  `/bin/nano` is a standalone nano-style editor service, `EDIT <path>` is the
  shell command, first implementation is `.txt` only, and later `BASIC <path>`
  plus `ASM <path>` call nano with removable language plugins/services. The
  editor implementation lives under `source/editor`; `source/bin/bin_nano.c` is
  the `/bin` service adapter. GNU nano is a behavior reference only, not source
  to import.
- `/bin` services follow the registry in `source/bin/bin_registry.c`; use
  `docs/SPRINT_005_BIN_SERVICE_ABI_AND_PIPES.md` for DOS-style service and pipe
  planning. Services are firmware-linked and build-selectable for now, not
  loadable workspace executables.
- Sprint 007 shell YMODEM transfer and Sprint 008 guarded C3COM execution
  precede ASM editor work. ASM nano mode is tracked in
  `docs/SPRINT_009_ASM_NANO_MODE_TASK_LIST.md`.
- ASM capture follows the editor/plugin boundary; native C3COM execution is now
  guarded by header validation, CRC, explicit `RUN`, and standard I/O ABI
  callbacks.
- `/bin/term` is implemented as an output-only ANSI/VT100 `/bin` service, not a
  shell built-in. BASIC `TERM "..."` is implemented through a dedicated safe
  service kind that can only invoke the `/bin/term` command family.
- `EDIT`, `/bin/nano`, and `BASIC` may launch without a filename. Untitled text
  saves to the first free `/data/untitled-N.txt`; untitled BASIC saves to the
  first free `/basic/untitled-N.bas`; clean `:q` creates no file and dirty `:q`
  requires save or discard.
- BLE HID keyboard support has a compiled input boundary and boot-keyboard mapper, but real keyboard pairing remains hardware-pending.

## Protected Recovery Invariant

- The computer has a protected system side and a maker-owned workspace side.
- Boot and the protected shell/`RENEW` path must not depend on files stored in `/workspace`.
- User commands may create, delete, or damage workspace files, but must not be able to delete or replace `RENEW`.
- If `workspace_fs` is healthy, the shell enters normal workspace mode.
- If `workspace_fs` is damaged, the shell still starts from the protected path and tells the user to run `RENEW`.
- `RENEW` asks twice, formats only `workspace_fs`, recreates the workspace layout, and leaves the protected system side untouched.
- Do not silently auto-format workspace during boot; recovery must be intentional.

## Canonical Architectural Contract

### Source anchors

- `docs/02_Software_Architecture.md`
- `docs/03_User_Experience_Guide.md`

### 02 Software Architecture (core)

- Layered architecture with strict single responsibility:
  - Computer (runtime), Shell, Workspace, Native RISC-V ASM Engine, BASIC Language, BASIC Library, Maker Programs.
- Lower layers provide services; upper layers do not depend on implementation details.
- Reference platform is ESP32-C3 + ESP-IDF, with portability goals across C-series targets.
- Separation of concerns and stable interfaces are mandatory.
- Recoverability: system software remains protected, workspace is recoverable.
- Architectural baseline for planning:
  - Shell is default environment.
  - Workspace supports editing and running text-based programs.
  - Native assembly is first-class and inspectable.

### 03 User Experience Guide (behavior)

- READY-state behavior:
  - Boot into a calm, familiar prompt (`READY.`).
- UX tone:
  - Clear, brief, honest, non-judgemental.
  - Avoid noise, avoid exaggerated wording.
- Error/warning behavior:
  - Errors explain what/where; avoid numeric-only codes.
  - Warnings only for potentially destructive actions, with explicit confirmation.
- Consistency:
  - Keep command behavior simple and predictable.
  - Return to READY after successful operations when practical.
- First impression:
  - power-on should enable immediate creation and exploration quickly.
