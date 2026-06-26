# Sprint 1: Recovery-first Ticket List (Fine-grained Review Units)

Use this as the execution queue and review ledger. Keep each ticket small and verify before moving to the next.

Status terms:

- `DONE (code-reviewed)`: implementation is present in `Old_version/` and has been checked against the ticket acceptance text.
- `PARTIAL`: implementation is present but one acceptance detail still needs cleanup or proof.
- `TODO`: not implemented.

## Sprint 1 – Recovery-first

### R1-T1 — Partition spec lock

Status: DONE (code-reviewed)

- File: `Old_version/partitions.csv`
- Action:
  1. Replace single `storage` partition with `system_fs` and `workspace_fs`.
  2. Confirm no overlap with `nvs`, `otadata`, `app0`.
- Done when:
  - `partitions.csv` contains both labels exactly.
  - Sizes and offsets are non-overlapping.
  - `storage` label is not present.
- Evidence:
  - `Old_version/partitions.csv` defines `system_fs` at `0x150000` size `0x100000`.
  - `Old_version/partitions.csv` defines `workspace_fs` at `0x250000` size `0x1B0000`.
  - No `storage` partition label remains.

### R1-T2 — Storage constants and API contract

Status: DONE (code-reviewed)

- File: `Old_version/main/storage.h`
- Action:
  1. Define mount points and partition labels.
  2. Add API for workspace renewal.
- Done when:
  - `STORAGE_SYSTEM_MOUNT_POINT` is declared.
  - `STORAGE_WORKSPACE_MOUNT_POINT` is declared.
  - `storage_renew_workspace()` is declared.
- Evidence:
  - `Old_version/main/storage.h` declares `/system`, `/workspace`, `system_fs`, and `workspace_fs`.
  - `Old_version/main/storage.h` declares `storage_renew_workspace()`.

### R1-T3 — Add dual filesystem mount logic

Status: DONE (code-reviewed)

- File: `Old_version/main/storage.c`
- Action:
  1. Add mount helper for system partition (read-mostly).
  2. Add mount helper for workspace partition (read/write, recoverable).
  3. Ensure system mount is isolated from workspace mount sequence.
- Done when:
  - Storage init tries both mounts.
  - Logs show whether each mount succeeded.
- Evidence:
  - `Old_version/main/storage.c` has separate `storage_mount_system()` and `storage_mount_workspace()` helpers.
  - `storage_init()` attempts system mount first, then workspace mount.
  - System mount failure is logged as warning; workspace mount failure is logged as error and stops init.

### R1-T4 — Workspace directory bootstrap

Status: DONE (code-reviewed)

- File: `Old_version/main/storage.c`
- Action:
  1. Add directory create list: `basic`, `asm`, `bin`, `config`, `data`, `temp`.
  2. Keep compatibility alias strategy for legacy uppercase locations if needed.
- Done when:
  - All six canonical directories exist after boot.
  - Existing directory creation failures are handled safely.
- Evidence:
  - `Old_version/main/storage.c` creates canonical `basic`, `asm`, `bin`, `config`, `data`, and `temp`.
  - Compatibility aliases `BASIC`, `ASM`, `CONFIG`, and `TEMP` are also created.
  - Existing directories return `ESP_OK`; conflicting non-directories fail safely.

### R1-T5 — Path resolution contract

Status: DONE (code-reviewed)

- File: `Old_version/main/storage.c`
- Action:
  1. Route unqualified names to `/workspace/basic/<name>.bas`.
  2. Keep absolute path handling under workspace mount.
  3. Block traversal (`..`) in all resolver paths.
- Done when:
  - `SAVE TEST` resolves to `workspace/basic/TEST.bas`.
  - `SAVE /asm/test.asm` stays in workspace mount space.
  - traversal attempts are rejected.
- Evidence:
  - `storage_resolve_path()` routes simple names to `/workspace/basic/<name>.bas`.
  - Absolute paths are prefixed with `/workspace`.
  - Any input containing `..` is rejected.

### R1-T6 — Workspace-only renew implementation

Status: DONE (code-reviewed)

- File: `Old_version/main/storage.c`
- Action:
  1. Implement `storage_renew_workspace()`.
  2. Unmount workspace only.
  3. Format only `workspace_fs`.
  4. Remount and re-bootstrap workspace directories.
- Done when:
  - System partition is not affected.
  - Workspace layout is recreated deterministically.
- Evidence:
  - `storage_renew_workspace()` unregisters only `workspace_fs`.
  - It formats only `STORAGE_WORKSPACE_PARTITION_LABEL`.
  - It remounts workspace and reruns workspace directory bootstrap.

### R1-T7 — Two-step RENEW command

Status: DONE (code-reviewed)

- File: `Old_version/main/shell.c`
- Action:
  1. Add `RENEW` command parsing.
  2. Add first destructive-warning prompt.
  3. Add second confirmation prompt.
  4. Call `storage_renew_workspace()` only after confirmation.
- Done when:
  - `RENEW` without `Y` then `Y` aborts cleanly.
  - `RENEW` with both confirmations resets workspace only.
- Evidence:
  - `shell.c` dispatches `RENEW` to `shell_renew_workspace()`.
  - `shell_renew_workspace()` prints a destructive warning and requires two `Y` confirmations.
  - `storage_renew_workspace()` is called only after both confirmations pass.

### R1-T8 — Shell status/error contract for renew/paths

Status: DONE (code-reviewed)

- File: `Old_version/main/shell.c`
- Action:
  1. Keep non-crashing error path for bad filenames and bad paths.
  2. Surface explicit renewal failure messages.
  3. Preserve command loop after any error.
- Done when:
  - Shell remains responsive after invalid commands.
  - Errors are short and non-numeric.
- Evidence:
  - Bad directory paths print `Bad path`.
  - Bad file names print `Bad filename`.
  - Renew failure prints `RENEW failed`.
  - Command handlers return to the shell loop after these error paths.

### R1-T9 — Storage log and boot observability

Status: DONE (code-reviewed)

- File: `Old_version/main/storage.c`
- Action:
  1. Add explicit log lines for mount/format/remount outcomes.
  2. Distinguish system mount and workspace mount status in logs.
- Done when:
  - Logs clearly identify partition/system/workspace paths and outcomes.
- Evidence:
  - Boot logs distinguish system mount and workspace mount outcomes.
  - Renew errors distinguish unmount, format, remount, and layout failures.
  - Renew success logs now distinguish unmount, format, remount, and layout recreation steps.

### R1-T10 — Command surface documentation sync

Status: DONE (code-reviewed)

- Files:
  - `Old_version/README.md`
  - `README.md`
  - `docs/05_File_System.md`
  - `docs/COMPLIANCE_MATRIX.md`
- Action:
  1. Add `RENEW` to command lists.
  2. Document `system_fs` and `workspace_fs`.
  3. Update compliance item status notes for partition-split behavior.
- Done when:
  - Docs and code command surface match.
  - Recovery-first behavior is explicitly named in user-facing docs.
- Evidence:
  - `Old_version/main/shell.c` includes `RENEW` in `HELP`.
  - `Old_version/README.md` lists `RENEW`.
  - `README.md` documents `system_fs`, `workspace_fs`, and workspace-only renew behavior.
  - `docs/05_File_System.md` documents `system_fs`, `workspace_fs`, and `RENEW`.
  - `docs/COMPLIANCE_MATRIX.md` records recoverability and partition-split status.

## Execution note

Stop after each completed ticket and review the result against this file before the next ticket.
