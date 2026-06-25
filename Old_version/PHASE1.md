# Phase 1 Notes

This firmware slice is the first executable step for the project:

- USB serial shell
- LittleFS mount
- BASIC program storage
- `DIR`, `LOAD`, `SAVE`, `DELETE`, `NEW`, `RUN`
- numbered BASIC lines entered from the PC terminal

Supported BASIC statements in this first pass:

- `PRINT`
- `LET`
- `INPUT`
- `IF ... THEN <line>`
- `GOTO`
- `GOSUB`
- `RETURN`
- `FOR`
- `NEXT`
- `END`

The implementation is intentionally small and can be extended toward the full design docs in later phases.

Hardware-backed acceptance:

- `tools/dir_delete_smoke.py`
- `tools/load_list_run_smoke.py`
- `tools/reboot_persistence_smoke.py`

All three regressions pass on the ESP32-C3 USB Serial/JTAG interface. Phase 2
starts with `ASM` / `ENDASM` source capture and strict unsupported-instruction
errors before native execution is enabled.
