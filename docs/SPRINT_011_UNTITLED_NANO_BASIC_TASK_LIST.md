# Sprint 011 Task List: Untitled Nano and BASIC Launch

Status: Planned

Goal: let `EDIT`, `/bin/nano`, and `BASIC` start without a filename. A maker
should be able to open the editor first and decide the filename later, instead
of always naming the file before editing.

This sprint changes editor launch and save behavior only. It does not add new
BASIC syntax, ASM mode, raw-key editing, tabs, or multiple buffers.

## Command Contract

```text
EDIT
/bin/nano
BASIC
EDIT /data/name.txt
/bin/nano /data/name.txt
BASIC /basic/name.bas
```

When no filename is provided:

- `EDIT` and `/bin/nano` open an untitled text buffer.
- `BASIC` opens an untitled BASIC buffer.
- The editor title/status should show an untitled state.
- The buffer is dirty only after the user enters text.
- `:q` on a clean untitled buffer quits without asking.
- `:q` on a dirty untitled buffer refuses to quit and tells the user to save,
  discard with `:q!`, or use save-as.
- `:w` on an untitled buffer asks for a filename or uses a deterministic default
  filename, depending on what the implementation slice chooses.

## Filename Policy

Preferred first implementation:

- `EDIT` / `/bin/nano` default to `/data/untitled-1.txt`.
- `BASIC` defaults to `/basic/untitled-1.bas`.
- If the default exists, try `untitled-2`, `untitled-3`, and so on.
- Use the same path validation as explicit filename launches.
- Do not overwrite an existing default file silently.

Alternative later behavior:

- Add a `:saveas <path>` editor command.
- On `:w` from an untitled buffer, ask for the path interactively.

The first implementation may choose deterministic defaults before interactive
save prompts, because the current editor is line-oriented and should stay simple.

## Non-Goals

- No multi-buffer editing.
- No tabs.
- No file picker.
- No current-directory browser.
- No raw-key prompt UI.
- No automatic save on quit.
- No BASIC immediate mode.
- No ASM editor mode in this sprint.

## Task List

### U11-T1 - Freeze untitled editor contract

- [ ] Update:
  - `docs/SPRINT_004_NANO_EDITOR_SERVICE.md`
  - `docs/04_Shell_Reference.md`
  - `README.md`
  - `source/bin/README.md`
- [ ] Document `EDIT`, `/bin/nano`, and `BASIC` as valid without a filename.
- [ ] Document the default naming policy:
  - `/data/untitled-N.txt`
  - `/basic/untitled-N.bas`
- [ ] Document quit/save behavior for clean and dirty untitled buffers.
- [ ] Pass criteria:
  - Docs do not claim prompt-based save-as unless it is implemented.
  - Docs keep explicit path behavior unchanged.

### U11-T2 - Extend editor request model for untitled buffers

- [ ] File: `source/editor/editor_service.h`.
- [ ] Add request state for optional path / untitled launch.
- [ ] Keep explicit path launches unchanged.
- [ ] Pass criteria:
  - Existing `EDIT /data/name.txt` and `BASIC /basic/name.bas` still compile.
  - Untitled mode is represented explicitly, not by a fake user path string.

### U11-T3 - Add default filename allocation

- [ ] File: `source/editor/editor_service.c`.
- [ ] Add default path selection:
  - text: `/data/untitled-1.txt`, then increment.
  - BASIC: `/basic/untitled-1.bas`, then increment.
- [ ] Reuse existing path policy and traversal rejection.
- [ ] Reject if no free default name is found within a bounded range.
- [ ] Pass criteria:
  - Existing files are never overwritten silently.
  - Default path remains inside `/workspace`.

### U11-T4 - Allow no-path launches in `/bin/nano`, `EDIT`, and `BASIC`

- [ ] File: `source/bin/bin_nano.c`.
- [ ] Allow:
  - `/bin/nano`
  - `EDIT`
  - `BASIC`
- [ ] Keep wrong extra arguments rejected.
- [ ] Pass criteria:
  - `/bin/nano` opens untitled text mode.
  - `EDIT` opens untitled text mode.
  - `BASIC` opens untitled BASIC mode.
  - `EDIT /data/name.txt` and `BASIC /basic/name.bas` still work.

### U11-T5 - Save and quit behavior

- [ ] File: `source/editor/editor_service.c`.
- [ ] Ensure:
  - clean untitled `:q` exits.
  - dirty untitled `:q` refuses and explains save/discard choices.
  - dirty untitled `:q!` discards.
  - untitled `:w` saves to the selected default filename.
  - after save, editor title/status uses the real path.
- [ ] Pass criteria:
  - No silent data loss.
  - No save outside `/data` or `/basic`.

### U11-T6 - Add smoke coverage

- [ ] Add or extend editor smoke coverage:
  - `EDIT`, text line, `:wq`, `CAT /data/untitled-1.txt`.
  - `BASIC`, valid numbered line, `:wq`, `CAT /basic/untitled-1.bas`.
  - clean `EDIT`, `:q`.
  - dirty `EDIT`, `:q` refused, `:q!` exits.
- [ ] Pass criteria:
  - Smoke fails shell-visibly on wrong default names or unsafe quit behavior.
  - Existing nano and BASIC smokes still pass.

### U11-T7 - Build and board verification

- [ ] Build:
  ```bash
  tools/idf53.sh -B build-c3-root build
  ```
- [ ] Board smoke:
  ```bash
  tools/idf53.sh -B build-c3-root -p /dev/ttyACM0 flash
  python3 tools/nano_editor_smoke.py --port /dev/ttyACM0 --timeout 25
  python3 tools/basic_editor_smoke.py --port /dev/ttyACM0 --timeout 25
  ```
- [ ] Pass criteria:
  - Build succeeds.
  - Untitled text and BASIC saves are board-tested.
  - Existing explicit filename behavior has no regression.

### U11-T8 - Completion docs and handoff

- [ ] Update:
  - `README.md`
  - `SESSION_STATUS.md`
  - `docs/SPRINT_004_NANO_EDITOR_SERVICE.md`
  - `source/bin/README.md`
- [ ] Record board evidence.
- [ ] Set next candidate milestone back to Sprint 010 T10-T4 unless terminal
  BASIC bridge is already complete.

## Execution Order

1. U11-T1
2. U11-T2
3. U11-T3
4. U11-T4
5. U11-T5
6. U11-T6
7. U11-T7
8. U11-T8

Keep Going.
