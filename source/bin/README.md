# C3 Workspace Bin Services

`source/bin` contains firmware-backed services exposed through the `/bin`
command path. These are not shell built-ins. The root firmware registers this
command handler during startup; the standalone shell does not link this folder.

The model is DOS-like: services have canonical `/bin/<name>` entries and may
also have shell aliases. Firmware currently links services at build time; later
native-runtime work may add real loadable workspace programs.

List linked services:

```text
/bin list
```

Current terminal-facing services:

```text
/bin/nano /data/note.txt
EDIT /data/note.txt
/bin/hardware gpio in -p <gpio> [--pull none|up|down|updown]
/bin/hardware gpio out -p <gpio> [-v 0|1] [--open-drain]
/bin/hardware gpio read -p <gpio>
/bin/hardware gpio write -p <gpio> -v 0|1
/bin/hardware gpio toggle -p <gpio>
/bin/hardware adc read -p <gpio>
/bin/hardware i2c config -sda <gpio> -scl <gpio> [-f <hz>] [--pullups]
/bin/hardware i2c probe -a <addr>
/bin/hardware i2c scan
/bin/hardware spi config -mosi <gpio> -miso <gpio> -sclk <gpio> [-cs <gpio>] [-f <hz>] [-m <mode>]
/bin/hardware spi xfer -tx <hexbytes>
HARDWARE gpio read -p <gpio>
```

## Nano Editor Service

`/bin/nano` is a firmware-linked line editor service. `EDIT` is the shell alias
for the same service.

Open a text file:

```text
EDIT /data/note.txt
/bin/nano /data/note.txt
```

Open a BASIC source file in nano BASIC mode:

```text
BASIC /basic/hello.bas
```

BASIC mode accepts `/basic/*.bas`, validates numbered BASIC lines before save,
and keeps BASIC runtime commands inside the editor session. `:run` saves and
runs the current numbered BASIC buffer using the full 64 KiB nano text buffer.
`:debug` remains planned for step-run. Future BASIC shell/hardware calls must use a safe
whitelist for query operations such as `PWD`, `LS`, `CAT`, and typed hardware
reads, while blocking destructive commands such as `RENEW`, `RM`, `WRITE`, `CP`,
`MV`, and native execution.

Supported editor commands:

```text
Text line  Append text
:w         Save
:q         Quit if clean
:q!        Quit without saving
:wq        Save and quit
:p         Print buffer
:clear     Clear buffer
:run       Save and run BASIC program in BASIC mode
:help      Help
```

Current limits and failure behavior:

- `EDIT` accepts only `/data/*.txt`.
- `BASIC` accepts only `/basic/*.bas`.
- Text buffer is 64 KiB per open file, including line separators.
- Input line buffer is 64 KiB, bounded by the same editor capacity.
- If allocation fails, nano prints `Out of memory` and returns a shell error.
- If an existing file is larger than the editor buffer, nano prints `File too large`.
- If appended text exceeds the editor buffer, nano prints `Buffer full` and returns to the shell.
- Thai text and Thai filenames under `/data` have been board-tested.

Board smoke:

```bash
python3 tools/nano_editor_smoke.py --port /dev/ttyACM0 --timeout 25
```

Build-time service selection defaults:

```c
CONFIG_C3_SERVICE_HARDWARE=1
CONFIG_C3_SERVICE_NANO=1
```

Set either macro to `0` in the build configuration when a minimal firmware image
should omit that service.

First RAM-backed pipe:

```text
CAT /data/input.txt | WRITE /data/output.txt
```

The current pipe buffer is intentionally small. Larger data should wait for the
planned `/temp` file-backed pipe path.

Notes:

```text
GPIO output default level: 0
ADC default pins on ESP32-C3: GPIO0-GPIO4
I2C default frequency: 100000 Hz when -f is omitted
SPI default frequency: 1000000 Hz when -f is omitted
SPI default mode: 0 when -m is omitted
Protected pins: GPIO18 and GPIO19
```

GPIO18 and GPIO19 stay protected by the hardware package because they carry USB
Serial/JTAG on common ESP32-C3 boards.

The service calls the reusable C APIs in `source/hardware`. Future BASIC or
monitor runtimes should call those same APIs directly or use the shell command
API when a text command path is appropriate.

Root firmware build:

```bash
tools/idf53.sh -B build-c3-root build
```

Board smoke test after flashing:

```bash
python3 tools/bin_hardware_gpio_smoke.py --port /dev/ttyACM0 --pin 8 --seconds 10
python3 tools/bin_hardware_adc_smoke.py --port /dev/ttyACM0 --pin 0
python3 tools/bin_hardware_i2c_smoke.py --port /dev/ttyACM0 --sda 6 --scl 7
python3 tools/bin_hardware_spi_smoke.py --port /dev/ttyACM0 --mosi 4 --miso 5 --sclk 6 --cs 7
```

Manual board LED off command tested on the current ESP32-C3 board:

```text
/bin/hardware gpio out -p 8 -v 1
/bin/hardware gpio write -p 8 -v 1
```
