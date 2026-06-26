# ESP32-C3 Micro Linux Shell Rebuild Checklist

Use this checklist when moving the micro Linux shell into a clean ESP32-C3
firmware project.

## Files To Copy

- `shell.c`
- `shell.h`
- `c3_com.h`
- `c3_shell_sources.cmake`
- `port/input.h`
- `port/storage.h`
- `CMakeLists.txt`
- `main/CMakeLists.txt`
- `main/app_main.c`
- `main/input_serial.c`
- `main/storage_fatfs.c`
- `partitions.csv`
- `sdkconfig.defaults`
- `idf53.sh`
- `standalone_tools/smoke.py`
- `COMMAND_API.md`
- `README.md`
- `PORT_CONTRACT.md`
- `TEST_CASES.md`

## Project Files To Provide

- `input.h`
- input backend implementation, usually USB Serial/JTAG first
- `storage.h`
- storage backend implementation mounted at `/workspace`
- partition table with `system_fs` and `workspace_fs`
- ESP-IDF project `CMakeLists.txt`
- ESP-IDF component `CMakeLists.txt`
- optional external command handler if the parent firmware wants `/bin` services

## Build Checks

1. From `source/shell`, run `./idf53.sh -B build-standalone build`.
2. Flash to ESP32-C3 over USB Serial/JTAG with
   `./idf53.sh -B build-standalone -p /dev/ttyACM0 flash`.
3. Confirm boot prints:

```text
C3 BASIC COMPUTER
Version 0.1

READY.
>
```

4. Run `HELP` and confirm:

```text
HELP DF PWD LS CD MKDIR RMDIR CAT WRITE RM CP MV RECV SEND RUN EDIT
RENEW
```

5. Run `python3 standalone_tools/smoke.py --port /dev/ttyACM0`.
6. Run the test cases in `TEST_CASES.md` when changing shell behavior.

## Use Checks

- `PWD` starts at `/`.
- `LS` lists the default workspace directories.
- `CD`, `MKDIR`, `WRITE`, `CAT`, `CP`, `MV`, `RM`, `RMDIR`, and `RM -R`
  operate inside `/workspace`.
- `DF` reports workspace free space.
- `RECV` and `SEND` move files over YMODEM without leaving `/workspace`.
- `RUN /bin/name.com [args...]` runs only validated C3COM images.
- BASIC commands are rejected with `UNKNOWN COMMAND`.
- Standalone shell builds do not require `source/hardware` or `source/bin`.
- Parent firmware `/bin` services, such as `/bin/hardware`, are registered
  outside the shell core.

## Required Safety Checks

- `LS` prints directories as `/name` and files as `name`.
- `RM /` returns `Bad path`.
- `RM -R /` returns `Bad path`.
- `RMDIR` removes only empty directories.
- `MV` handles file and directory rename/move inside `/workspace`.
- `DIR`, `COPY`, `MOVE`, `DELETE`, `RENAME`, and BASIC commands return
  `UNKNOWN COMMAND`.
- Bare `RUN` prints guarded runner usage instead of entering BASIC.
- `RENEW` requires two confirmations and formats only `workspace_fs`.
