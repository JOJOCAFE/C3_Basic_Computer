# ESP32-C3 Hardware Terminal Commands

This file documents the current terminal-facing hardware command client.

Hardware commands are not shell built-ins. They are `/bin` services registered
by the root firmware above the lean micro Linux shell. The reusable service API
lives in `source/hardware`; the text command adapter lives in `source/bin`.

## Current Commands

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

## Output

Successful commands print a confirmation line and `OK`.

Examples:

```text
GPIO8 OUT 1
OK
```

```text
GPIO8 WRITE 0
OK
```

```text
GPIO8 READ 1
OK
```

```text
ADC GPIO0 RAW 1234 MV 1000
OK
```

```text
I2C CONFIG SDA 6 SCL 7 100000HZ PULLUPS ON
OK
```

```text
SPI RX FF
OK
```

Invalid input prints:

```text
Bad input
```

GPIO service failures print:

```text
GPIO failed
```

ADC, I2C, and SPI service failures print `ADC failed`, `I2C failed`, or
`SPI failed`.

## Examples

Set GPIO8 as output and drive it low:

```text
/bin/hardware gpio out -p 8 -v 0
```

Drive GPIO8 high:

```text
/bin/hardware gpio write -p 8 -v 1
```

Read GPIO8:

```text
/bin/hardware gpio read -p 8
```

Configure GPIO4 as input with internal pullup:

```text
/bin/hardware gpio in -p 4 --pull up
```

Toggle GPIO8:

```text
/bin/hardware gpio toggle -p 8
```

Read ADC1 from GPIO0:

```text
/bin/hardware adc read -p 0
```

Configure I2C on GPIO6/GPIO7 with internal pullups, probe address `0x3C`, then
scan the bus:

```text
/bin/hardware i2c config -sda 6 -scl 7 -f 100000 --pullups
/bin/hardware i2c probe -a 0x3C
/bin/hardware i2c scan
```

Configure SPI and transfer one byte:

```text
/bin/hardware spi config -mosi 4 -miso 5 -sclk 6 -cs 7 -f 1000000 -m 0
/bin/hardware spi xfer -tx 9F
```

## Tested Board LED

On the current ESP32-C3 board, GPIO8 controls the blue LED. The off level is
`1`, so this sequence turns it off:

```text
/bin/hardware gpio out -p 8 -v 1
/bin/hardware gpio write -p 8 -v 1
```

## Boundary

Implemented now:

- GPIO terminal commands through `/bin/hardware gpio`
- GPIO C API for firmware callers
- ADC one-shot terminal reads through `/bin/hardware adc`
- I2C config/probe/scan terminal commands through `/bin/hardware i2c`
- SPI config/transfer terminal commands through `/bin/hardware spi`
- ADC, I2C, and SPI C APIs for firmware callers

Deferred terminal commands:

- I2C raw read/write/register helpers
- SPI mode helpers beyond one full-duplex transfer command
- BASIC statements/functions; BASIC should call `source/hardware` directly
