# ESP32-C3 Hardware Package Task List

Status: COMPLETE - implemented as standalone service package, root firmware is
wired through `source/bin`, generic GPIO terminal commands are board-tested,
and ADC/I2C/SPI terminal adapters are wired for build/smoke validation

This package must stay separate from `source/shell`. The shell can remain a
lean standalone micro Linux shell, while C3 firmware, BASIC, future `/bin`
services, and monitor code can link this package when hardware access is
needed.

## Design Rules

- Do not add hardware code to `source/shell`.
- Do not add hardware commands to shell `HELP`. Hardware terminal commands are
  `/bin` services above the shell, not shell built-ins.
- Keep the first package as a reusable C API, not a shell parser.
- Treat `/bin` services as clients above the shell. The shell may later
  launch or call those tools, but hardware is not part of the shell core.
- Protect recovery pins by default, especially USB Serial/JTAG pins GPIO18 and
  GPIO19.
- Use ESP-IDF driver APIs, not direct register access.
- Keep ESP32-C3 support first, but make the naming portable enough for later C5,
  C6, or P4 ports.

## H0 - Package Skeleton

Status: DONE

Files:

- `README.md`
- `PIN_POLICY.md`
- `c3_hardware_sources.cmake`
- `hardware.h`

Done when:

- The package purpose and build boundary are documented.
- `c3_hardware_sources.cmake` lists all source/include paths needed by a parent
  ESP-IDF component.
- `hardware.h` exposes one common init/reset/status surface.

## H1 - GPIO API

Status: DONE

Files:

- `hardware_gpio.h`
- `hardware_gpio.c`

Done when:

- API supports digital input, output, read, write, toggle, and pull modes.
- API validates GPIO numbers using ESP-IDF GPIO validity macros.
- API rejects protected pins unless an explicit unsafe override is passed.
- GPIO18/GPIO19 are protected by default because they carry USB Serial/JTAG on
  ESP32-C3.

## H2 - ADC API

Status: DONE

Files:

- `hardware_adc.h`
- `hardware_adc.c`

Done when:

- API supports one-shot analog read by GPIO pin.
- API maps GPIO to ADC unit/channel through ESP-IDF ADC helpers.
- First ESP32-C3 implementation accepts ADC1 pins only by default: GPIO0-GPIO4.
- API reports raw ADC value and optionally calibrated millivolts when
  calibration is available.

## H3 - I2C API

Status: DONE

Files:

- `hardware_i2c.h`
- `hardware_i2c.c`

Done when:

- API supports bus config with SDA, SCL, frequency, and internal pullup option.
- API supports probe/scan, read register, write register, and raw write-read.
- Command vocabulary is compatible with ESP-IDF/Linux-style `i2cconfig`,
  `i2cdetect`, `i2cget`, `i2cset`, and `i2cdump` for future shell tools.
- ESP32-C3 implementation assumes one I2C controller.

## H4 - SPI API

Status: DONE

Files:

- `hardware_spi.h`
- `hardware_spi.c`

Done when:

- API supports bus config with MISO, MOSI, SCLK, optional CS, frequency, and
  SPI mode 0-3.
- API supports a bounded full-duplex transfer.
- Implementation uses `SPI2_HOST` on ESP32-C3 and must not touch flash SPI.
- Protected pins are rejected by default.

## H5 - `/bin` / BASIC Boundary

Status: DONE

Files:

- `README.md`
- `COMMANDS.md`
- `docs/SPRINT_003_BASIC_HARDWARE_API.md`

Done when:

- Docs explain that `/bin` services, BASIC, and monitor code should call this
  package API.
- Docs explain that `/bin/hardware gpio`, `adc`, `i2c`, and `spi` are current
  terminal-facing service clients outside the lean shell core.
- Docs explain that BASIC should call `source/hardware` directly, not route
  through shell built-ins or `/bin/hardware` text commands.

## H6 - Board Verification

Status: DONE

Files:

- `VERIFICATION.md`
- `tools/bin_hardware_gpio_smoke.py`
- `tools/bin_hardware_adc_smoke.py`
- `tools/bin_hardware_i2c_smoke.py`
- `tools/bin_hardware_spi_smoke.py`

Done when:

- Root firmware builds with `source/hardware` and `source/bin`.
- Standalone `source/shell` still builds without hardware/bin.
- ESP32-C3 is flashed with the root firmware.
- Generic GPIO terminal commands work through the board shell.
- GPIO8 off command is confirmed manually with `/bin/hardware gpio write`.
- ADC/I2C/SPI command adapters build and have host-side smoke scripts.

## Verification

Completed root firmware build:

```bash
tools/idf53.sh -B build-c3-root build
```

Completed standalone shell boundary build:

```bash
cd source/shell
./idf53.sh -B build-standalone build
```

Completed board checks:

```bash
python3 tools/workspace_shell_smoke.py --port /dev/ttyACM0
python3 tools/no_basic_shell_smoke.py --port /dev/ttyACM0
python3 tools/adversarial_shell_smoke.py --port /dev/ttyACM0
python3 tools/bin_hardware_gpio_smoke.py --port /dev/ttyACM0 --pin 8 --seconds 10
python3 tools/bin_hardware_adc_smoke.py --port /dev/ttyACM0 --pin 0
python3 tools/bin_hardware_i2c_smoke.py --port /dev/ttyACM0 --sda 6 --scl 7
python3 tools/bin_hardware_spi_smoke.py --port /dev/ttyACM0 --mosi 4 --miso 5 --sclk 6 --cs 7
```
