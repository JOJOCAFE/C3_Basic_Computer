# Sprint 005 Planning: `/bin` Service ABI and Pipes

Status: Registry, stream type, one RAM-backed pipe, and board smoke passed;
`/temp` file-backed pipes pending.

Goal: make C3 services feel like DOS-style commands: each service can run from
the shell, expose a canonical `/bin/<name>` command, optionally provide a shell
alias, and later participate in pipelines.

## Current Implemented Model

Firmware-linked services:

```text
/bin/hardware
/bin/nano
```

Shell aliases:

```text
HARDWARE
EDIT
```

Service discovery:

```text
/bin list
```

Current source ownership:

```text
source/bin      service registry and /bin adapters
source/editor   editor core used by /bin/nano
source/hardware hardware API used by /bin/hardware
source/shell    command parser and external service dispatch
```

## Service Contract

Each service has:

```text
canonical name
alias list
exec(args, io)
removable flag
```

The current C API is intentionally close to a future `main(argc, argv, io)`
shape, but still receives raw argument text while the command parser remains
small.

## Build-Time Selection

Services are firmware-linked today. A minimal image can omit services by setting
build macros:

```text
CONFIG_C3_SERVICE_HARDWARE=0
CONFIG_C3_SERVICE_NANO=0
```

Protected recovery must stay outside this system:

```text
RENEW
HELP
PWD
LS
CD
MKDIR
RMDIR
CAT
WRITE
RM
CP
MV
```

Those are shell/recovery commands, not removable `/bin` services.

## Pipe ABI

Pipes use a small RAM buffer first. Services now have a stream type foundation,
while existing command handlers still bridge through `shell_exec_io_t`.

Target service shape:

```text
service_main(argc, argv, stdin, stdout, stderr)
```

Target shell examples:

```text
CAT /data/input.txt | /bin/filter | WRITE /data/output.txt
/bin/hardware adc read -p 0 | /bin/log /data/adc.txt
```

Implemented first pipe:

```text
CAT /data/input.txt | WRITE /data/output.txt
```

## Task List

### S5-T1 - Add `bin_stream_t`

- [x] Add read/write callback stream type.
- [x] Add service I/O shape for stdin/stdout/stderr.
- [x] Keep compatibility with current `shell_exec_io_t`.

### S5-T2 - Convert `/bin/nano` output to service streams

- [x] Bridge `/bin/nano` through the shared service I/O path.
- [x] Preserve current `EDIT /data/name.txt` behavior.
- [ ] Convert editor internals fully to `bin_stream_t` after raw-key input exists.
- [x] Board-tested `EDIT /data/test.txt` and direct `/bin/nano /data/test.txt`
  equivalents through `tools/nano_editor_smoke.py`.

### S5-T3 - Convert `/bin/hardware` output to service streams

- [x] Bridge `/bin/hardware` through the shared service I/O path.
- [x] Preserve current GPIO/ADC/I2C/SPI command behavior.
- [ ] Convert hardware internals fully to `bin_stream_t` after pipe coverage is board-tested.
- [x] Board-tested `/bin/hardware gpio` on GPIO8.

### S5-T4 - Add shell parser support for one pipe

- [x] Parse one `|`.
- [x] Reject malformed or multiple pipe commands.
- [x] Feed left command output to right command stdin.
- [x] Support `CAT /data/in.txt | WRITE /data/out.txt`.
- [x] Board-tested `CAT /data/in.txt | WRITE /data/out.txt`.

### S5-T5 - Add RAM-backed pipe buffer

- [x] Add fixed 2048-byte RAM pipe capture.
- [x] Detect pipe overflow and return an error.

### S5-T6 - Add `/temp` file-backed pipe plan

- [x] Document `/temp` file-backed pipes as later work.
- [ ] Implement file-backed pipes after RAM pipe behavior is board-tested.

### S5-T7 - Board smoke

- [x] Add `tools/bin_pipe_smoke.py`.
- [x] Flash current firmware.
- [x] Run `/bin list`.
- [x] Run `EDIT /data/test.txt`.
- [x] Run `/bin/nano /data/test.txt`.
- [x] Run `HARDWARE gpio read -p 8`.
- [x] Run `CAT /data/in.txt | WRITE /data/out.txt`.
- [x] 2026-06-27 check: blocked because no `/dev/ttyACM*` or `/dev/ttyUSB*`
  serial device was visible to the host.
- [x] 2026-06-27 retry: `dmesg` briefly reported `cdc_acm ... ttyACM0: USB ACM device`,
  but `/dev/ttyACM0` disappeared before flash could open it. Treat next session
  as USB passthrough/host exposure first, then firmware smoke.
- [x] 2026-06-27 board verification: host-level USB access exposed `/dev/ttyACM0`;
  user was not in `dialout`, so flash/smokes required elevated serial access.
- [x] 2026-06-27 board verification: `/bin list` returned `/bin/hardware removable`
  and `/bin/nano removable`.
- [x] 2026-06-27 board verification: direct `/bin/nano /data/name.txt` opened,
  saved, and returned to the shell.
- [x] 2026-06-27 board verification: `HARDWARE gpio read -p 8` returned
  `GPIO8 READ 0` and `OK`.
- [x] 2026-06-27 board verification: `python3 tools/bin_pipe_smoke.py --port /dev/ttyACM0`
  passed.
- [x] 2026-06-27 board verification: `python3 tools/bin_hardware_gpio_smoke.py --port /dev/ttyACM0 --pin 8 --seconds 10`
  passed.
- [x] Fix recorded during board verification: the RAM pipe buffer now lives on
  heap instead of the main task stack.

Commands to run when the ESP32-C3 board is connected:

```bash
tools/idf53.sh -C . -B build-c3-root -p /dev/ttyACM0 flash
python3 tools/workspace_shell_smoke.py --port /dev/ttyACM0
python3 tools/nano_editor_smoke.py --port /dev/ttyACM0
python3 tools/bin_pipe_smoke.py --port /dev/ttyACM0
python3 tools/bin_hardware_gpio_smoke.py --port /dev/ttyACM0 --pin 8 --seconds 10
```

## Non-Goals For Now

- No loadable `.com` files yet.
- No native execution from workspace files yet.
- No removable protected recovery commands.
- No multi-stage pipes yet.
- No file-backed pipes yet.
