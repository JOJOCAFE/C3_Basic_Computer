# ESP32-C3 Micro Linux Shell Test Cases

These cases define expected shell behavior. Board smoke scripts implement the
current executable subset.

## Normal Workflow

| Case | Commands | Expected result | Covered by |
| --- | --- | --- | --- |
| Show commands | `HELP` | Lists `HELP DF PWD LS CD MKDIR RMDIR CAT WRITE RM CP MV RECV SEND RUN` and `RENEW`. | `no_basic_shell_smoke.py`, `adversarial_shell_smoke.py` |
| Show workspace free space | `DF` | Prints Unix-style `Filesystem 1K-blocks Used Available Mounted on` output for `/`. | `workspace_shell_smoke.py` |
| Show current directory | `PWD` | Prints `/` at boot and current path after `CD`. | `workspace_shell_smoke.py` |
| List root | `LS` | Shows workspace directories with `/` prefix. | `workspace_shell_smoke.py` |
| Create and enter directory | `MKDIR temp/name`, `CD temp/name` | Returns `OK`; `PWD` shows `/temp/name`. | `workspace_shell_smoke.py` |
| Create and read file | `WRITE note.txt text`, `CAT note.txt` | File is created and text is printed. | `workspace_shell_smoke.py` |
| Copy file | `CP note.txt copy.txt` | Destination file exists with same content. | `workspace_shell_smoke.py` |
| Move file | `MV copy.txt moved.txt` | Source disappears; destination exists. | `workspace_shell_smoke.py` |
| Move directory | `MV move-dir moved-dir` | Directory and child files move together. | `workspace_shell_smoke.py` |
| Receive text file | `RECV /basic/demo.bas` plus YMODEM send | File is saved byte-for-byte under `/workspace/basic`. | `ymodem_transfer_smoke.py` |
| Receive binary file | `RECV /bin/test.com` plus YMODEM send | Binary file is saved byte-for-byte under `/workspace/bin`. | `ymodem_transfer_smoke.py` |
| Send file | `SEND /basic/demo.bas` plus YMODEM receive | Host receives the exact file size and bytes. | `ymodem_transfer_smoke.py` |
| Run C3COM file | `RUN /bin/tool.com one two` | Valid C3COM header is checked, argv is passed, stdout prints, and exit code is reported. | `c3com_fixture_smoke.py` |
| Remove file | `RM note.txt` | File is removed. | `workspace_shell_smoke.py` |
| Remove empty directory | `MKDIR empty-dir`, `RMDIR empty-dir` | Empty directory is removed. | `workspace_shell_smoke.py` |
| Remove tree | `RM -R tree` | Directory subtree is removed. | `workspace_shell_smoke.py` |
| Renew workspace | `RENEW`, `Y`, `Y` | Workspace is formatted and layout recreated. | `renew_full_smoke.py` |

## Break / Destructive Cases

| Case | Commands | Expected result | Covered by |
| --- | --- | --- | --- |
| BASIC not exposed | `PRINT 1+2`, `10 PRINT "X"`, `NEW`, `LIST`, `LOAD X`, `SAVE X` | `UNKNOWN COMMAND`; no reboot. | `no_basic_shell_smoke.py`, `adversarial_shell_smoke.py` |
| Legacy aliases not exposed | `DIR`, `COPY A B`, `MOVE A B`, `DELETE X`, `RENAME A B` | `UNKNOWN COMMAND`. | `no_basic_shell_smoke.py`, `adversarial_shell_smoke.py` |
| Bad text commands | `THIS IS NOT A COMMAND`, `#$%^&*`, `123ABC` | `UNKNOWN COMMAND`; prompt returns. | `adversarial_shell_smoke.py` |
| Bare RUN | `RUN` | Prints `Usage: RUN /bin/name.com [args...]`; does not enter BASIC. | `no_basic_shell_smoke.py`, `adversarial_shell_smoke.py` |
| Workspace root delete | `RM /`, `RM -R /` | `Bad path`; workspace root is not removed. | `adversarial_shell_smoke.py` |
| Non-empty directory delete | `RMDIR moved-dir` with child file present | `RMDIR failed`; child data remains. | `workspace_shell_smoke.py` |
| Directory without recursive flag | `RM moved-dir` | `Is directory`. | Covered by command contract; add direct smoke if changed. |
| Recursive containment | `RM -R ../../BAD` | Must not remove outside workspace. | Covered by path guards; add direct smoke if changed. |
| Move directory into itself | `MV dir dir/child` | `Bad path`. | Covered by command contract; add direct smoke if changed. |
| Overlong line | 1400-character command | `UNKNOWN COMMAND`; prompt returns. | `adversarial_shell_smoke.py` |
| Binary-ish input | Null/escape/invalid byte sequence | Prompt returns; shell remains responsive. | `adversarial_shell_smoke.py` |
| RENEW abort | `RENEW`, `N` | `CANCELLED`; shell remains responsive. | `adversarial_shell_smoke.py` |
| Transfer overwrite guard | `RECV /basic/demo.bas` when file exists | Returns `Exists`; overwrite requires `RECV -F`. | `ymodem_transfer_smoke.py` |
| Transfer path escape | `RECV ../../BAD` or `SEND ../../BAD` | `Bad path`; no file outside workspace is touched. | `ymodem_transfer_smoke.py` |

## Parent Firmware `/bin` Service Cases

These cases apply to the root `C3_Basic_Computer` firmware, not to the
standalone shell project.

| Case | Commands | Expected result | Covered by |
| --- | --- | --- | --- |
| GPIO output mode | `/bin/hardware gpio out -p 8 -v 0` | `GPIO8 OUT 0`, then `OK`. | `bin_hardware_gpio_smoke.py` |
| GPIO write high | `/bin/hardware gpio write -p 8 -v 1` | `GPIO8 WRITE 1`, then `OK`. | `bin_hardware_gpio_smoke.py` |
| GPIO write low | `/bin/hardware gpio write -p 8 -v 0` | `GPIO8 WRITE 0`, then `OK`. | `bin_hardware_gpio_smoke.py` |
| Blue LED off on current board | `/bin/hardware gpio out -p 8 -v 1`, `/bin/hardware gpio write -p 8 -v 1` | GPIO8 is configured high and the board LED turns off. | Manual board check |
| ADC read | `/bin/hardware adc read -p 0` | `ADC GPIO0 RAW ...`, then `OK`. | `bin_hardware_adc_smoke.py` |
| I2C config | `/bin/hardware i2c config -sda 6 -scl 7 --pullups` | `I2C CONFIG ...`, then `OK`. | `bin_hardware_i2c_smoke.py` |
| I2C scan | `/bin/hardware i2c scan` | `I2C SCAN ...`, then `OK`. | `bin_hardware_i2c_smoke.py` |
| SPI config | `/bin/hardware spi config -mosi 4 -miso 5 -sclk 6 -cs 7` | `SPI CONFIG ...`, then `OK`. | `bin_hardware_spi_smoke.py` |
| SPI transfer | `/bin/hardware spi xfer -tx 9F` | `SPI RX ...`, then `OK`. | `bin_hardware_spi_smoke.py` |

## Command API Cases

| Case | API call | Expected result | Covered by |
| --- | --- | --- | --- |
| Command lookup | `shell_command_from_name("LS", &command)` | Returns `SHELL_STATUS_OK` and `SHELL_COMMAND_LS`. | Standalone boot self-test |
| BASIC rejection | `shell_command_from_name("PRINT", &command)` | Returns `SHELL_STATUS_UNKNOWN_COMMAND`. | Standalone boot self-test |
| Status names | `shell_status_name(SHELL_STATUS_NOT_ALLOWED)` | Returns `NOT_ALLOWED`. | Standalone boot self-test |
| Bad input | `shell_exec_line(NULL, &io)`, `shell_exec_line("", &io)` | Returns `SHELL_STATUS_BAD_INPUT` and `SHELL_STATUS_EMPTY`. | Standalone boot self-test |
| Captured output | `shell_exec_line("PWD", &io)` with write callback | Returns `SHELL_STATUS_OK`; callback receives `/`. | Standalone boot self-test |
| Enum dispatch | `shell_exec_command(SHELL_COMMAND_HELP, NULL, &io)` | Returns `SHELL_STATUS_OK`; callback receives command list. | Standalone boot self-test |
| Protected destructive call | `shell_exec_line("RENEW", &io)` without flags | Returns `SHELL_STATUS_NOT_ALLOWED`; no format occurs. | Standalone boot self-test |

## Required Regression Set

Before accepting shell changes, run:

```bash
python3 -m py_compile \
  tools/workspace_shell_smoke.py \
  tools/no_basic_shell_smoke.py \
  tools/renew_full_smoke.py \
  tools/adversarial_shell_smoke.py \
  tools/bin_hardware_gpio_smoke.py \
  tools/ymodem_transfer_smoke.py \
  tools/c3com_fixture_smoke.py

tools/idf53.sh -B build-c3-root build

python3 tools/workspace_shell_smoke.py --port /dev/ttyACM0
python3 tools/no_basic_shell_smoke.py --port /dev/ttyACM0
python3 tools/renew_full_smoke.py --port /dev/ttyACM0
python3 tools/adversarial_shell_smoke.py --port /dev/ttyACM0
python3 tools/ymodem_transfer_smoke.py --port /dev/ttyACM0
python3 tools/c3com_fixture_smoke.py --smoke --port /dev/ttyACM0
python3 tools/bin_hardware_gpio_smoke.py --port /dev/ttyACM0 --pin 8 --seconds 10
```
