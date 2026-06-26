# Sprint 003 BASIC Hardware API

Status: Design for review

Scope: design the BASIC-facing hardware API that calls `source/hardware`
directly. This sprint does not add shell built-ins, does not route BASIC
through `/bin/hardware`, and does not modify the active hardware service
implementation.

`Old_version/` is legacy reference only. The current source boundaries are:

```text
source/shell     standalone lean workspace shell
source/bin       terminal-facing /bin services
source/hardware  reusable firmware hardware C API
main/basic.*     BASIC runtime and library integration point
```

## Goal

Give BASIC programs controlled access to GPIO and ADC first, with a design path
for I2C and SPI later.

BASIC should feel like a small computer language:

```basic
PINMODE 8, OUTPUT
DWRITE 8, 1
PRINT DREAD(8)
PRINT AREAD(0)
```

The implementation must keep one rule clear:

```text
BASIC hardware calls -> BASIC hardware adapter -> source/hardware C API
```

It must not use:

```text
BASIC hardware calls -> shell text command -> /bin/hardware -> source/hardware
```

`source/bin` remains a terminal command adapter. BASIC gets its own typed C
adapter over the same hardware service layer.

## Layering

### Existing lower layer

`source/hardware` owns board validation and ESP-IDF driver calls:

- `hardware_init()`
- `hardware_reset()`
- `hardware_gpio_configure_input()`
- `hardware_gpio_configure_output()`
- `hardware_gpio_read()`
- `hardware_gpio_write()`
- `hardware_gpio_toggle()`
- `hardware_adc_read_gpio()`
- `hardware_i2c_*()`
- `hardware_spi_*()`

This layer stays independent from BASIC and from the shell.

### Existing terminal adapter

`source/bin` owns `/bin/hardware` terminal adapters.

It is useful for manual testing and PC-terminal use, but it is not the BASIC
runtime API. BASIC should not format text commands and feed them to `/bin`.

### New BASIC adapter layer

Add a small future adapter owned by the BASIC runtime, for example:

```text
main/basic_hardware.h
main/basic_hardware.c
```

or, if the runtime is later split:

```text
source/basic/basic_hardware.h
source/basic/basic_hardware.c
```

The adapter responsibilities are:

1. Convert BASIC values into typed C config structs.
2. Call `source/hardware` APIs directly.
3. Convert `esp_err_t` failures into BASIC runtime errors.
4. Preserve BASIC line context in error messages.
5. Keep the protected-pin policy visible and conservative.

The adapter should not parse shell syntax, print `/bin` command output, or know
about terminal prompts.

## C API Design

The BASIC adapter should expose a narrow C API to the BASIC evaluator.

Proposed first-pass header:

```c
typedef enum {
    BASIC_HW_OK = 0,
    BASIC_HW_ERR_BAD_ARGUMENT,
    BASIC_HW_ERR_PROTECTED_PIN,
    BASIC_HW_ERR_UNSUPPORTED_PIN,
    BASIC_HW_ERR_NOT_CONFIGURED,
    BASIC_HW_ERR_DRIVER,
} basic_hw_status_t;

typedef enum {
    BASIC_HW_PIN_INPUT = 0,
    BASIC_HW_PIN_INPUT_PULLUP,
    BASIC_HW_PIN_INPUT_PULLDOWN,
    BASIC_HW_PIN_OUTPUT,
    BASIC_HW_PIN_OUTPUT_OPEN_DRAIN,
} basic_hw_pin_mode_t;

typedef struct {
    basic_hw_status_t status;
    esp_err_t driver_error;
    const char *message;
} basic_hw_result_t;

basic_hw_result_t basic_hw_init(void);
basic_hw_result_t basic_hw_pinmode(int pin, basic_hw_pin_mode_t mode);
basic_hw_result_t basic_hw_dwrite(int pin, int level);
basic_hw_result_t basic_hw_dread(int pin, int *level_out);
basic_hw_result_t basic_hw_toggle(int pin);
basic_hw_result_t basic_hw_aread(int gpio, int *raw_out,
                                  int *millivolts_out,
                                  bool *millivolts_valid_out);
```

Rules:

- Call `hardware_init()` before the first hardware operation.
- Return typed status to the BASIC runtime; do not print from this adapter.
- Keep all output formatting in the BASIC error/reporting path.
- Use `source/hardware` validation rather than duplicating ESP-IDF pin rules.
- Default all protected-pin override fields to false.

The BASIC runtime can later add a convenience function:

```c
const char *basic_hw_status_name(basic_hw_status_t status);
```

That is for diagnostics only. Logic should branch on the enum, not strings.

## BASIC Surface

### Sprint 003 minimum surface

Statements:

```text
PINMODE pin, mode
DWRITE pin, level
DTOGGLE pin
```

Functions:

```text
DREAD(pin)
AREAD(gpio)
```

Constants:

```text
INPUT
INPUT_PULLUP
INPUT_PULLDOWN
OUTPUT
OUTPUT_OPEN_DRAIN
LOW
HIGH
```

Examples:

```basic
10 PINMODE 8, OUTPUT
20 DWRITE 8, HIGH
30 PRINT DREAD(8)
40 END
```

```basic
PINMODE 4, INPUT_PULLUP
PRINT DREAD(4)
```

```basic
PRINT AREAD(0)
```

`AREAD(gpio)` returns raw ADC counts. Calibrated millivolts are intentionally
not part of the first BASIC function return value because BASIC currently has
no structured return type.

### Later surface, not Sprint 003 implementation

Possible later statements and functions:

```text
AMV(gpio)             return calibrated millivolts when valid, otherwise error
I2COPEN sda, scl, hz
I2CCLOSE
I2CPROBE(address)
I2CWRITE address, byte...
I2CREAD(address, register, count)
SPIOPEN mosi, miso, sclk, cs, hz, mode
SPICLOSE
SPIXFER(byte...)
```

These need more language design because BASIC needs byte arrays, bounded data
buffers, and clear formatting for multi-byte results before they are pleasant to
use.

## Parsing Policy

The BASIC parser should treat hardware words as library statements/functions,
not shell commands.

Parsing rules:

- `PINMODE` requires exactly two arguments.
- `DWRITE` requires exactly two arguments.
- `DTOGGLE` requires exactly one argument.
- `DREAD()` requires exactly one argument.
- `AREAD()` requires exactly one argument.
- Pin and level arguments must evaluate to integers.
- `LOW` maps to `0`; `HIGH` maps to `1`.
- Mode constants are identifiers, not strings.
- String aliases such as `"LED"` are deferred.

Rejected examples:

```basic
PINMODE 8
DWRITE 8, 2
PRINT DREAD()
PRINT AREAD("A0")
```

## Error Model

Errors must be short, deterministic, and line-anchored when the program is
running from stored BASIC source.

Recommended user-facing formats:

```text
Line 20: bad hardware argument
Line 30: protected pin
Line 40: unsupported pin
Line 50: hardware driver failed
```

Interactive immediate mode can omit the line prefix:

```text
bad hardware argument
protected pin
unsupported pin
hardware driver failed
```

Mapping:

```text
ESP_ERR_INVALID_ARG      -> BASIC_HW_ERR_BAD_ARGUMENT or UNSUPPORTED_PIN
ESP_ERR_INVALID_STATE    -> BASIC_HW_ERR_NOT_CONFIGURED
ESP_ERR_NOT_SUPPORTED    -> BASIC_HW_ERR_UNSUPPORTED_PIN
protected-pin rejection  -> BASIC_HW_ERR_PROTECTED_PIN
other esp_err_t          -> BASIC_HW_ERR_DRIVER
```

The BASIC runtime should stop the current program on a hardware error. It
should not keep executing after a failed pin configuration or failed write.

Diagnostics for developers may include the raw `esp_err_t`, but the normal user
message should stay stable across ESP-IDF versions.

## Pin Policy

Sprint 003 must inherit `source/hardware/PIN_POLICY.md`.

Default rules:

- GPIO18 and GPIO19 stay protected.
- BASIC has no unsafe protected-pin override in the first release.
- GPIO input/output validity is delegated to `source/hardware`.
- ADC defaults to ESP32-C3 ADC1 GPIO0-GPIO4 only.
- `AREAD()` returns raw counts only.
- I2C and SPI protected-pin overrides remain unavailable from BASIC until a
  later explicit review.

This means these should fail in BASIC:

```basic
PINMODE 18, OUTPUT
DWRITE 19, HIGH
```

The first BASIC hardware release should prioritize keeping USB Serial/JTAG
available for recovery over giving advanced users every possible pin escape.

## Tests

### Host parser tests

Add parser/evaluator tests before board tests:

1. `PINMODE 8, OUTPUT` parses as a hardware statement.
2. `DWRITE 8, HIGH` parses level `1`.
3. `DWRITE 8, LOW` parses level `0`.
4. `DREAD(8)` parses as an integer function.
5. `AREAD(0)` parses as an integer function.
6. Missing arguments fail deterministically.
7. Non-integer pin arguments fail deterministically.
8. Invalid level values fail deterministically.
9. Unknown mode identifiers fail deterministically.

### C adapter tests

Where host-side unit tests can mock `source/hardware`, verify:

1. `basic_hw_pinmode()` calls input config for input modes.
2. `basic_hw_pinmode()` calls output config for output modes.
3. `basic_hw_dwrite()` rejects values other than `0` and `1`.
4. `basic_hw_dread()` writes the output level only on success.
5. `basic_hw_aread()` returns raw ADC value.
6. Protected-pin errors map to `BASIC_HW_ERR_PROTECTED_PIN`.
7. Generic driver errors map to `BASIC_HW_ERR_DRIVER`.

### Firmware build tests

Required build checks:

```bash
tools/idf53.sh -B build-c3-root build
cd source/shell
./idf53.sh -B build-standalone build
```

The standalone shell build must still pass without linking BASIC hardware
support into `source/shell`.

### Board smoke tests

Use GPIO8 only for the first visible board smoke because it is already the
tested LED pin on the current ESP32-C3 board.

Proposed smoke program:

```basic
10 PINMODE 8, OUTPUT
20 DWRITE 8, LOW
30 DWRITE 8, HIGH
40 PRINT DREAD(8)
50 END
```

Acceptance:

- Program runs without shell command dispatch.
- GPIO8 changes state.
- `DREAD(8)` prints `0` or `1`.
- Shell prompt returns after the program ends.
- Existing `/bin/hardware` smoke tests still pass.
- Existing workspace shell smoke tests still pass.

Protected-pin smoke:

```basic
10 PINMODE 18, OUTPUT
20 END
```

Acceptance:

```text
Line 10: protected pin
```

or equivalent short protected-pin error.

## Implementation Tasks

### S3-T1 - Freeze BASIC hardware syntax

- [ ] Update BASIC library docs after review.
- [ ] Decide whether `DTOGGLE` belongs in the first release.
- [ ] Keep string aliases deferred.

Done when:

- The accepted Sprint 003 statement/function list is written in the language
  docs.

### S3-T2 - Add BASIC hardware adapter header

- [ ] Add typed status enum.
- [ ] Add pin mode enum.
- [ ] Add adapter function declarations.

Done when:

- The header compiles without shell or `/bin` dependencies.

### S3-T3 - Implement BASIC hardware adapter

- [ ] Call `hardware_init()` lazily or during BASIC runtime init.
- [ ] Map pin modes to `hardware_gpio_*` configs.
- [ ] Map ADC reads to `hardware_adc_read_gpio()`.
- [ ] Map errors into BASIC hardware statuses.

Done when:

- No adapter function constructs a shell command string.
- No adapter function calls `source/bin`.

### S3-T4 - Add BASIC parser hooks

- [ ] Parse `PINMODE`, `DWRITE`, and `DTOGGLE` as statements.
- [ ] Parse `DREAD()` and `AREAD()` as functions.
- [ ] Add constants for modes and levels.

Done when:

- Parser tests pass for valid and invalid hardware syntax.

### S3-T5 - Add runtime execution hooks

- [ ] Execute hardware statements through the BASIC adapter.
- [ ] Evaluate hardware functions through the BASIC adapter.
- [ ] Stop program execution on hardware error.

Done when:

- Runtime errors are line-anchored.
- Immediate mode errors are short and deterministic.

### S3-T6 - Add regression and board smoke tests

- [ ] Add host parser tests.
- [ ] Add adapter tests or focused smoke tests if mocks are not practical.
- [ ] Add a board smoke for GPIO8.
- [ ] Add a protected-pin negative smoke.

Done when:

- Root firmware build passes.
- Standalone shell build still passes.
- Existing shell and `/bin/hardware` smokes still pass.
- New BASIC GPIO smoke passes on board.

## What Not To Implement Yet

Do not implement these in Sprint 003:

- BASIC access through shell built-ins.
- BASIC access by invoking `/bin/hardware` text commands.
- Unsafe protected-pin override from BASIC.
- String pin aliases such as `"LED"`.
- PWM / analog output.
- Interrupt callbacks.
- Background pin watching.
- Debounce helpers.
- I2C BASIC commands.
- SPI BASIC commands.
- Multi-byte BASIC array return values for bus transfers.
- A general hardware permission system.
- A board-profile alias database.
- Changes to `source/shell`.
- Changes to the existing `source/hardware` API unless implementation proves a
  small missing primitive is unavoidable.

## Review Questions

1. Should `DTOGGLE pin` be included in the first BASIC release, or should the
   first release stay limited to `PINMODE`, `DWRITE`, `DREAD()`, and `AREAD()`?
2. Should calibrated ADC millivolts be exposed as `AMV(gpio)` later, or should
   `AREAD()` eventually return millivolts instead of raw counts?
3. Should BASIC ever expose protected-pin override, or should that remain only
   in C and possibly terminal `/bin` tools?
4. Should pin aliases wait for a board-profile document, or can a tiny built-in
   alias table start with `LED` after GPIO8 is confirmed across target boards?

## Completion Gate

Sprint 003 is complete only when:

1. BASIC hardware syntax is documented.
2. BASIC calls `source/hardware` directly through a typed C adapter.
3. The shell remains standalone and lean.
4. `/bin/hardware` remains a terminal client, not a BASIC dependency.
5. Protected pins remain blocked from BASIC.
6. Root firmware build passes.
7. Standalone shell build passes.
8. Existing shell smoke tests pass.
9. Existing `/bin/hardware` smoke tests pass.
10. New BASIC hardware smoke passes on the board.
