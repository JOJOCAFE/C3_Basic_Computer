# Phase 1 Fix List (from Compliance Matrix)

Status: Historical recovery-first fix list. It references `Old_version/`
because it predates the root project move. Current active implementation lives
at the repository root, with shell code in `source/shell`, hardware services in
`source/hardware`, and `/bin` adapters in `source/bin`.

Goal: remove high-risk behavior before assembly work, starting with recoverability.

## Scope (Sprint 1)

Keep Sprint 1 recovery-first:
- prevent workspace corruption from bricking system
- separate system and workspace storage
- make `renew` deterministic and safe

## 0) Recovery-first foundation (must run first)

- Files: `Old_version/partitions.csv`, `Old_version/sdkconfig.defaults`, `Old_version/sdkconfig`
- Split data persistence into two LittleFS partitions:
  - `system_fs` for protected system payloads
  - `workspace_fs` for user editable files
- Acceptance:
  - `partitions.csv` defines both partitions.
  - partition labels and mount names are documented in code.
  - one partition can be erased without affecting the other.

## 1) Storage mount split and partition boundaries

- Files: `Old_version/main/storage.h`, `Old_version/main/storage.c`
- Add two VFS mount points and labels:
  - system mount (read-mostly, protected)
  - workspace mount (full user operations)
- Acceptance:
  - `storage_init()` mounts both partitions.
  - command/data paths explicitly route to workspace unless system-only path.

## 2) Workspace partition recovery (`renew`)

- Files: `Old_version/main/storage.c`, `Old_version/main/shell.c`
- Implement `RENEW` as:
  - wipe/reformat workspace partition only
  - leave system partition and app runtime untouched
- Acceptance:
  - two-step confirmation flow as documented
  - confirms workspace rebuild with expected `/basic`, `/asm`, `/bin`, `/config`, `/data`, `/temp` present

3) Core shell command handlers

- File: `Old_version/main/shell.c`
- Add/finish `help`, `pwd`, `ls`, `cat`, `rm`, `mkdir`, `rmdir`, `cd`.
- Acceptance:
  - command behavior is deterministic and predictable.
  - unknown commands are explicit, safe rejects (not hard faults).

## 4) Error contract cleanup

- Files: `Old_version/main/shell.c`, `Old_version/main/basic.c`
- Normalize recoverability-first messaging:
  - show what failed and where
  - avoid ambiguous `BASIC ERROR` without context
- Acceptance:
  - invalid commands and parse failures are plain text and bounded.

## 5) Canonical path/layout policy

- Files: `Old_version/main/storage.c`, `Old_version/main/shell.c`
- Enforce `/basic`, `/asm`, `/bin`, `/config`, `/data`, `/temp` in workspace.
- Keep compatibility with legacy uppercase locations temporarily if present.
- Acceptance:
  - listing and save/load use canonical workspace paths.

## 6) Listing and path consistency

- File: `Old_version/main/shell.c`
- Ensure `ls`/`dir` and path resolution have consistent outputs and error shape.
- Acceptance:
  - root and subfolder listing stable across commands.
  - no ambiguous path mapping behavior.

## 7) System command baseline

- File: `Old_version/main/shell.c`
- Implement `version`, `memory`, `date`, `time`, `diagnostics`, plus explicit update placeholder.
- Acceptance:
  - all are discoverable via `help <command>`.
  - commands are safe no-op where functionality is staged.

## 8) Monitor scaffolding

- File: `Old_version/main/shell.c`
- Add monitor command placeholders for `reg`, `mem`, `dump`, `disasm`, `step`, `break` with safe responses.
- Acceptance:
  - no crashes, no undefined behavior.
  - clear “not yet supported” messaging.

## 9) Close docs and status

- Files: `README.md`, `docs/COMPLIANCE_MATRIX.md`, `docs/IMPLEMENTATION_CHECKLIST.md`
- Reflect that Sprint 1 is recovery-first and partition-split-based.
- Acceptance:
  - baseline status is updated.

## Execution order

0 → 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9
