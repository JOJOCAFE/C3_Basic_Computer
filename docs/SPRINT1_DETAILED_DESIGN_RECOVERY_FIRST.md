# Sprint 1 Detailed Design: Recovery-first, Dual-partition Workspace Architecture

## Purpose

This document is a review-first design for Sprint 1, focused on making command-out-of-scope behavior safe by separating immutable system data from recoverable workspace data.

The goal is to prevent the pattern:

`invalid command -> unknown error -> accidental storage corruption -> apparent brick / stuck restore`

and replace it with:

`invalid command -> bounded error -> recoverable state -> controlled workspace rebuild`.

## 0) What this sprint is for

Sprint 1 is explicitly recoverability-first.

1. Prevent workspace data corruption from impacting system runtime.
2. Ensure `RENEW` is deterministic and destructive only where intended.
3. Preserve command stability and keep boot + READY loop stable.
4. Keep behavior discoverable and documented for maker-facing review.

This design intentionally narrows scope before assembly runtime work.

## 1) Design constraints and non-goals

1. No core hardware changes.
2. No BASIC parser or assembly engine redesign in Sprint 1.
3. No filesystem migration framework beyond the two-partition boundary and path migration behavior.
4. No full UX rewrite; preserve tiny-shell, immediate responses, and minimal noise.
5. No destructive user-facing commands without explicit confirmation.

## 2) Existing behavior summary to preserve

1. Boot enters shell on `C3 BASIC COMPUTER`, `Version 0.1`, then shows `READY.`.
2. USB Serial/JTAG remains transport.
3. Existing basic program lifecycle remains: line store, list, run, save/load/delete.
4. No monitor or assembly runtime behavior is introduced in this sprint.
5. Error style stays text-first and short.

## 3) Target architecture

### 3.1 Partition boundaries

1. `app0` remains OTA application partition (unchanged layout owner).
2. `system_fs` (read-mostly) contains firmware-supporting runtime payload expectations.
3. `workspace_fs` (read-write) contains user files and scratch state.
4. System and workspace must be independently mountable.
5. `RENEW` must be implemented only on `workspace_fs`.
6. The protected shell/renew path must not depend on files stored in `workspace_fs`.

### 3.2 Runtime mount points

1. `system_fs` mount point is `/system`.
2. `workspace_fs` mount point is `/workspace`.
3. Command resolver and workspace commands default to `/workspace`.
4. Legacy compatibility: keep any uppercase aliases needed for existing user habits without weakening safety.

### 3.3 Mount policy

1. `system_fs` mount:
   - read-only from shell context.
   - not used for mutable operations.
2. `workspace_fs` mount:
   - writable.
   - format-on-fail for first boot or damaged partition.
3. Failure policy:
   - if `workspace_fs` fails badly, do not attempt to silently format `system_fs`.
   - do not silently format `workspace_fs` at boot.
   - start the protected shell path even when `workspace_fs` cannot mount or bootstrap.
   - tell the user to run `RENEW` for controlled workspace repair.
   - `storage_init` can fail hard only if the protected runtime path itself cannot start.

## 4) File system contract

### 4.1 Workspace layout

1. `/workspace/basic`
2. `/workspace/asm`
3. `/workspace/bin`
4. `/workspace/config`
5. `/workspace/data`
6. `/workspace/temp`

### 4.2 Compatibility and migration

1. Canonical read/write path is lowercase layout above.
2. Uppercase compatibility aliases may be created by initialization or path resolver for continuity.
3. Any future migration from uppercase to lowercase should be explicit and user-visible (warn or note).

### 4.3 Recoverability semantics

1. `RENEW` deletes and recreates only `workspace_fs` content.
2. `system_fs` contents must remain untouched by any workspace recovery flow.
3. `RENEW` is an administrative command and should be two-step confirmed.
4. User files must not be able to delete or replace the `RENEW` command.
5. A corrupted workspace should degrade to protected recovery shell, not a brick.

## 5) Detailed task decomposition

Each task is intentionally small enough to review.

### Task 1: Freeze partition spec and sizes

1. Update partition table source (`partitions.csv`) with explicit `system_fs` and `workspace_fs`.
2. Keep OTA sections unchanged.
3. Ensure partition offsets/sizes are aligned and non-overlapping.
4. Acceptance:
   - build uses custom partition file.
   - no overlap with `nvs`, `otadata`, `app0`.

### Task 2: Declare storage contract in header

1. Add mount constants and partition labels to storage header.
2. Add workspace renew API (`storage_renew_workspace`).
3. Keep generic storage API for path resolving.
4. Acceptance:
   - single storage header owns constants for all modules.

### Task 3: Mount plan in storage initialization

1. Add system mount function using read-only flags.
2. Add workspace mount function using writable flags and recovery mount behavior.
3. Sequence:
   1. try mount system.
   2. mount workspace.
   3. prepare directory layout.
4. Acceptance:
   - boot logs indicate both mount operations attempted.
   - shell starts normally when workspace is healthy.
   - protected shell still starts when workspace is unhealthy, with `RENEW` available.

### Task 4: Directory bootstrap service

1. Create `/workspace` subdirectories for canonical names.
2. Keep uppercase compatibility aliases as non-critical extras.
3. Acceptance:
   - mandatory dirs exist after boot.
   - no crash if they pre-exist.

### Task 5: Workspace path resolver contract

1. Default resolver route all relative paths into `/workspace`.
2. If path has no `/` and no `.` suffix, default to `/workspace/basic/<name>.bas`.
3. Absolute paths remain absolute under `/workspace`.
4. Reject `..` traversal.
5. Acceptance:
   - `SAVE TEST` still stores expected file.
   - `LOAD /basic/TODO.BAS` and `LOAD TODO.BAS` both behave predictably.

### Task 6: Implement controlled workspace renew

1. Add unmount + format on `workspace_fs`.
2. Remount workspace and re-run directory bootstrap.
3. Return explicit outcomes for:
   - mount fail
   - format fail
   - layout fail
4. Keep command-level logs clear and short.
5. Acceptance:
   - only workspace storage is deleted.
   - system mount remains available after renew.
   - workspace is not auto-formatted before the two confirmations.

### Task 7: Add RENEW command flow in shell

1. Parse `RENEW` keyword in command dispatch.
2. Show warning block exactly once before first confirmation.
3. Ask first confirmation question.
4. Ask second confirmation question.
5. Call storage renew only after both pass.
6. Return `OK` if success.
7. Acceptance:
   - abort at either stage leaves state unchanged.

### Task 8: Error and status contract updates

1. Replace vague or non-actionable error prints around storage commands.
2. For renew:
   - explicit warnings and confirmation prompts.
   - explicit failure reason on API return path.
3. For path command errors:
   - keep old style brevity.
   - never crash on malformed names.
4. Acceptance:
   - recoverable command failures do not stop shell loop.

### Task 9: Logging and diagnostics clarity

1. Add startup logs:
   - system mount status
   - workspace mount status
2. Add renew logs:
   - unmount attempt
   - format result
   - remount result
3. Acceptance:
   - logs allow user to distinguish `workspace` vs `system` failures quickly.

### Task 10: Documented command behavior alignment

1. Update root README to mention dual partition and recoverability model.
2. Update `05_File_System.md` storage partition section.
3. Update compliance evidence map to indicate which pass/fail items changed.
4. Ensure old README command list includes `RENEW`.
5. Acceptance:
   - docs and command behavior are no longer contradictory.

## 6) Execution order with review checkpoints

1. Checkpoint A: Partition spec lock
   - Review: size math, partition names, compatibility.
2. Checkpoint B: Mount contract
   - Review: read-only system, writable workspace, recovery behavior.
3. Checkpoint C: Path policy
   - Review: canonical layout + legacy fallback.
4. Checkpoint D: Renew command contract
   - Review: confirmation, safety boundaries, return text.
5. Checkpoint E: Documentation closure
   - Review: every behavior added is documented.

At each checkpoint, pause and confirm before moving to next.

## 7) Acceptance rubric (manual review first)

1. No write operation can target `/system`.
2. `RENEW` never erases system files.
3. Command out-of-scope behavior remains in shell and does not brick on next boot.
4. Workspace directories always recreate on boot.
5. User can intentionally recover data by `RENEW`, not accidentally.

## 8) Open risks and explicit mitigations

1. Risk: boot config still expects old partition labels.
   - Mitigation: update only storage layer and docs, keep partition table as source of truth.
2. Risk: existing users rely on legacy uppercase-only command paths.
   - Mitigation: keep compatibility aliases and plan migration note.
3. Risk: command prompt confusion due to path mismatch.
   - Mitigation: keep resolver deterministic and document canonical path policy.
4. Risk: renew command interpreted as app reset.
   - Mitigation: visible warnings and double confirmation.

## 9) "Out of scope" list to avoid scope creep

1. Full monitor command implementation.
2. Native assembly execution boundary.
3. Advanced file sync to microSD.
4. Partition filesystem encryption or wear-leveling policy changes beyond defaults.

## 10) Minimal deliverable definition for Sprint 1

Sprint 1 is complete when all tasks are applied and the user can verify this flow:

1. Boot.
2. Run file command and BASIC command without crashing.
3. Run `RENEW` and confirm both prompts.
4. Observe workspace reset, system remains unchanged.
5. Restart command path continues to function on `/workspace` layout.

This design is intended to be auditable line-by-line and can be implemented in small, isolated steps without touching interpreter semantics first.  
  
Keep Going.
