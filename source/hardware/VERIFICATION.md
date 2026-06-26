# ESP32-C3 Hardware Verification

This records the current completed hardware-service verification.

## Builds

Root firmware with `source/hardware` and `source/bin`:

```bash
tools/idf53.sh -B build-c3-root build
```

Result:

```text
PASS
```

Standalone shell boundary:

```bash
cd source/shell
./idf53.sh -B build-standalone build
```

Result:

```text
PASS
```

The standalone shell build confirms that `source/shell` remains lean and can
build without `source/hardware` or `source/bin`.

## Flash

Root firmware flashed to the ESP32-C3 board:

```bash
tools/idf53.sh -B build-c3-root -p /dev/ttyACM0 flash
```

Result:

```text
PASS
```

Board detected during flash:

```text
Chip: ESP32-C3 revision v0.4
USB mode: USB-Serial/JTAG
Port: /dev/ttyACM0
```

## Shell GPIO Smoke

Host smoke:

```bash
python3 tools/bin_hardware_gpio_smoke.py --port /dev/ttyACM0 --pin 8 --seconds 10
```

Result:

```text
PASS: /bin hardware gpio
```

The smoke test sends generic GPIO commands through the shell:

```text
/bin/hardware gpio out -p 8 -v 0
/bin/hardware gpio write -p 8 -v 1
/bin/hardware gpio write -p 8 -v 0
```

The repeated writes are a host-side test pattern. There is no dedicated LED
application or firmware-level LED command in `/bin`.

## ADC/I2C/SPI Adapter Build Check

Root firmware build after wiring `/bin/hardware adc`, `i2c`, and `spi`:

```bash
tools/idf53.sh -B build-c3-root build
```

Result:

```text
PASS
```

Standalone shell boundary build after wiring the adapters:

```bash
cd source/shell
./idf53.sh -B build-standalone build
```

Result:

```text
PASS
```

Host script syntax checks:

```bash
env PYTHONDONTWRITEBYTECODE=1 python3 -m py_compile \
  tools/bin_hardware_gpio_smoke.py \
  tools/bin_hardware_adc_smoke.py \
  tools/bin_hardware_i2c_smoke.py \
  tools/bin_hardware_spi_smoke.py
```

Result:

```text
PASS
```

Board smoke is pending for ADC/I2C/SPI because `/dev/ttyACM0` was not present
during this verification pass.

## Manual LED Off Test

The current board blue LED is controlled by GPIO8 and turns off at level `1`.

Manual shell commands:

```text
/bin/hardware gpio out -p 8 -v 1
/bin/hardware gpio write -p 8 -v 1
```

Observed shell output:

```text
GPIO8 OUT 1
OK

GPIO8 WRITE 1
OK
```

Observed board result:

```text
Blue LED off
```

## Regression Checks

Legacy LED app command scan:

```text
No matches in source, tools, main, README.md, or docs
```

Whitespace check:

```bash
git diff --check
```

Result:

```text
PASS
```
