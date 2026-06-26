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
