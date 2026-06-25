# Session Status — 2026-06-22

## Completed

- Standardized builds on ESP-IDF 5.3.2 and GCC 13.2.
- Added `tools/idf53.sh` to reject mismatched ESP-IDF versions.
- Clean-built in `build-idf53` and flashed `/dev/ttyACM0`.
- Passed:
  - `tools/dir_delete_smoke.py`
  - `tools/load_list_run_smoke.py`
  - `tools/reboot_persistence_smoke.py`
- Updated README with the implemented BASIC statement, expression, condition,
  math-function, constant, and shell-command reference.

## Resume Point

Start Phase 2 with `ASM` / `ENDASM` source capture:

1. Keep assembly source separate from the BASIC program.
2. Add line-specific unsupported-instruction errors.
3. Test assembler output independently.
4. Do not enable native IRAM execution until source capture and validation pass.

## Build and Flash

```bash
tools/idf53.sh -B build-idf53 build
tools/idf53.sh -B build-idf53 -p /dev/ttyACM0 flash
```
