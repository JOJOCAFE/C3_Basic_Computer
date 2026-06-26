# ESP32-C3 Hardware Service Package

`source/hardware` is a reusable C API for firmware services that need board
hardware access. It is intentionally separate from `source/shell`.

The lean shell remains a workspace shell. User-visible `/bin` services are
layered above the shell: the shell may launch or call those services, but GPIO,
ADC, I2C, and SPI are not shell core features.

## Package Contents

```text
c3_hardware_sources.cmake  Reusable source/include list for ESP-IDF components
hardware.h/.c              Common init, reset, status, and pin policy helpers
hardware_gpio.h/.c         Digital input, output, read, write, toggle, pulls
hardware_adc.h/.c          One-shot ADC read by GPIO
hardware_i2c.h/.c          One ESP32-C3 I2C master bus service
hardware_spi.h/.c          SPI2_HOST master service
COMMANDS.md                Current terminal-facing /bin/hardware commands
PIN_POLICY.md              Recovery pin rules
TASKS.md                   Package task list
VERIFICATION.md            Build, flash, and board-test record
```

## Build Boundary

Include this package from a parent ESP-IDF component. Do not add hardware
commands to `source/shell`.

```cmake
set(C3_HARDWARE_DIR "${CMAKE_CURRENT_LIST_DIR}/../source/hardware")
include("${C3_HARDWARE_DIR}/c3_hardware_sources.cmake")

idf_component_register(
    SRCS "main.c" ${C3_HARDWARE_SRCS}
    INCLUDE_DIRS "." ${C3_HARDWARE_INCLUDE_DIRS}
    REQUIRES ${C3_HARDWARE_REQUIRES}
)
```

The package uses ESP-IDF 5.3 driver APIs:

- `driver/gpio.h`
- `esp_adc/adc_oneshot.h`
- `esp_adc/adc_cali.h`
- `driver/i2c_master.h`
- `driver/spi_master.h`

## Service Model

Call `hardware_init()` before using the service layer. `hardware_reset()`
deinitializes owned I2C and SPI handles and clears the package init flag.

GPIO and ADC operations are per-call. I2C owns one ESP32-C3 I2C master bus.
SPI owns one `SPI2_HOST` device. Reconfiguring I2C or SPI replaces the previous
owned handle.

This API is intended for future clients such as:

- `/bin/hardware gpio`
- `/bin/hardware adc`
- `/bin/hardware i2c`
- `/bin/hardware spi`
- BASIC or monitor runtimes that call the same C service layer

Those clients are separate workspace commands/programs above the shell, not
shell built-ins.

The currently implemented terminal command client is `/bin/hardware`, with
GPIO, ADC, I2C, and SPI subcommands. It is provided by `source/bin`, not by
`source/shell`. The root firmware registers the `/bin` command handler during
startup; the standalone shell build does not include `source/bin` or
`source/hardware`.

## Safety Defaults

GPIO18 and GPIO19 are protected by default because they carry USB Serial/JTAG
on common ESP32-C3 development boards. GPIO, I2C, and SPI configuration APIs
reject those pins unless their explicit unsafe override is set.

ADC defaults to ESP32-C3 ADC1 GPIO0-GPIO4 only. The read result always reports
raw ADC counts. Calibrated millivolts are reported when ESP-IDF calibration is
available.

SPI uses `SPI2_HOST`, not flash SPI. Transfers are bounded to
`HARDWARE_SPI_MAX_TRANSFER_BYTES`.

## Current Terminal Commands

The root C3 firmware includes this package through `main/CMakeLists.txt` and
exposes generic hardware terminal calls through `source/bin`, not through shell
built-ins:

```text
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
```

See [`COMMANDS.md`](COMMANDS.md) for syntax, output, and examples.

## Verification

Root firmware build:

```bash
tools/idf53.sh -B build-c3-root build
```

Standalone shell boundary build:

```bash
cd source/shell
./idf53.sh -B build-standalone build
```

Board smoke tests:

```bash
python3 tools/workspace_shell_smoke.py --port /dev/ttyACM0
python3 tools/no_basic_shell_smoke.py --port /dev/ttyACM0
python3 tools/adversarial_shell_smoke.py --port /dev/ttyACM0
python3 tools/bin_hardware_gpio_smoke.py --port /dev/ttyACM0 --pin 8 --seconds 10
python3 tools/bin_hardware_adc_smoke.py --port /dev/ttyACM0 --pin 0
python3 tools/bin_hardware_i2c_smoke.py --port /dev/ttyACM0 --sda 6 --scl 7
python3 tools/bin_hardware_spi_smoke.py --port /dev/ttyACM0 --mosi 4 --miso 5 --sclk 6 --cs 7
```

See [`VERIFICATION.md`](VERIFICATION.md) for the current completed test record.
