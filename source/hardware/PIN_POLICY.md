# ESP32-C3 Pin Policy

This package protects recovery and console access first.

## Protected By Default

GPIO18 and GPIO19 are protected by default. On common ESP32-C3 boards they are
used for USB Serial/JTAG, which is the console and recovery path.

The default policy rejects GPIO18/GPIO19 in:

- GPIO input/output/pull/read/write/toggle calls
- I2C SDA/SCL configuration
- SPI MISO/MOSI/SCLK/CS configuration

Each C API that can touch protected pins has an explicit unsafe override.
Terminal-facing `/bin` clients must not hide that override. The current
`/bin/hardware` commands do not expose unsafe protected-pin access.

## GPIO

GPIO validation uses ESP-IDF GPIO macros:

- `GPIO_IS_VALID_GPIO()` for input-capable GPIO
- `GPIO_IS_VALID_OUTPUT_GPIO()` for output-capable GPIO

The current `/bin/hardware` service accepts ESP32-C3 GPIO numbers `0` through
`21`, then each C API applies ESP-IDF validity checks and the protected pin
policy. GPIO output defaults to level `0` when `-v` is not supplied.

Common board LED note: on the tested ESP32-C3 board, GPIO8 controls the blue
LED and the off level is `1`.

## ADC

The default ESP32-C3 ADC policy accepts ADC1 GPIO0-GPIO4 only. `hardware_adc`
maps GPIO to ADC unit/channel through `adc_oneshot_io_to_channel()`.

ADC reads always return raw counts. Millivolts are optional and valid only when
ESP-IDF calibration succeeds.

## I2C

ESP32-C3 uses one I2C master controller in this package. SDA and SCL must be
valid GPIOs and must not be protected pins unless explicitly overridden.

Internal pullups are available for bring-up, but real I2C buses should use
external pullups sized for the bus speed and capacitance.

## SPI

SPI uses `SPI2_HOST` only. It must not touch flash SPI. MISO, MOSI, SCLK, and
optional CS are validated through ESP-IDF GPIO macros and protected pin policy.

Transfers are bounded by `HARDWARE_SPI_MAX_TRANSFER_BYTES`.
