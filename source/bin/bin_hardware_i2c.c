#include "bin_internal.h"

#include "hardware.h"
#include "hardware_i2c.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define I2C_SCAN_FIRST_ADDR 0x03U
#define I2C_SCAN_LAST_ADDR 0x77U
#define I2C_SCAN_TIMEOUT_MS 30

static const char *skip_ws(const char *s)
{
    while (s && *s && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

static char *next_token(char **cursor)
{
    char *start = (char *)skip_ws(*cursor);
    if (!start || *start == '\0') {
        *cursor = start;
        return NULL;
    }

    char *end = start;
    while (*end && !isspace((unsigned char)*end)) {
        end++;
    }
    if (*end) {
        *end++ = '\0';
    }
    *cursor = end;
    return start;
}

static bool parse_uint32(const char *text, uint32_t *out)
{
    if (!text || !out) {
        return false;
    }

    char *end = NULL;
    unsigned long value = strtoul(text, &end, 0);
    if (end == text || *end != '\0' || value > UINT32_MAX) {
        return false;
    }

    *out = (uint32_t)value;
    return true;
}

static bool parse_gpio(const char *text, gpio_num_t *out)
{
    uint32_t value = 0;
    if (!parse_uint32(text, &value) || value > 21U || !out) {
        return false;
    }

    *out = (gpio_num_t)value;
    return true;
}

static bool parse_address(const char *text, uint8_t *out)
{
    uint32_t value = 0;
    if (!parse_uint32(text, &value) || value > 0x7fU || !out) {
        return false;
    }

    *out = (uint8_t)value;
    return true;
}

static shell_status_t i2c_bad_input(const shell_exec_io_t *io)
{
    bin_write(io, "Bad input\r\n");
    return SHELL_STATUS_BAD_INPUT;
}

static shell_status_t i2c_io_error(const shell_exec_io_t *io)
{
    bin_write(io, "I2C failed\r\n");
    return SHELL_STATUS_IO_ERROR;
}

static shell_status_t ensure_hardware_ready(const shell_exec_io_t *io)
{
    esp_err_t err = hardware_init();
    if (err != ESP_OK) {
        bin_write(io, "Hardware init failed\r\n");
        return SHELL_STATUS_IO_ERROR;
    }
    return SHELL_STATUS_OK;
}

static shell_status_t run_config(char *cursor, const shell_exec_io_t *io)
{
    gpio_num_t sda = GPIO_NUM_NC;
    gpio_num_t scl = GPIO_NUM_NC;
    uint32_t frequency_hz = HARDWARE_I2C_DEFAULT_FREQUENCY_HZ;
    bool enable_pullups = false;

    char *token;
    while ((token = next_token(&cursor)) != NULL) {
        if (strcasecmp(token, "-sda") == 0) {
            if (!parse_gpio(next_token(&cursor), &sda)) {
                return i2c_bad_input(io);
            }
        } else if (strcasecmp(token, "-scl") == 0) {
            if (!parse_gpio(next_token(&cursor), &scl)) {
                return i2c_bad_input(io);
            }
        } else if (strcasecmp(token, "-f") == 0) {
            if (!parse_uint32(next_token(&cursor), &frequency_hz) ||
                frequency_hz == 0U) {
                return i2c_bad_input(io);
            }
        } else if (strcasecmp(token, "--pullups") == 0) {
            enable_pullups = true;
        } else {
            return i2c_bad_input(io);
        }
    }

    if (sda == GPIO_NUM_NC || scl == GPIO_NUM_NC) {
        return i2c_bad_input(io);
    }

    shell_status_t status = ensure_hardware_ready(io);
    if (status != SHELL_STATUS_OK) {
        return status;
    }

    hardware_i2c_config_t config = hardware_i2c_default_config(sda, scl);
    config.frequency_hz = frequency_hz;
    config.enable_internal_pullups = enable_pullups;

    esp_err_t err = hardware_i2c_configure(&config);
    if (err != ESP_OK) {
        return i2c_io_error(io);
    }

    bin_writef(io, "I2C CONFIG SDA %d SCL %d %luHZ PULLUPS %s\r\nOK\r\n",
               (int)sda, (int)scl, (unsigned long)frequency_hz,
               enable_pullups ? "ON" : "OFF");
    return SHELL_STATUS_OK;
}

static shell_status_t run_probe(char *cursor, const shell_exec_io_t *io)
{
    bool have_address = false;
    uint8_t address = 0;

    char *token;
    while ((token = next_token(&cursor)) != NULL) {
        if (strcasecmp(token, "-a") == 0) {
            if (!parse_address(next_token(&cursor), &address)) {
                return i2c_bad_input(io);
            }
            have_address = true;
        } else {
            return i2c_bad_input(io);
        }
    }

    if (!have_address) {
        return i2c_bad_input(io);
    }

    shell_status_t status = ensure_hardware_ready(io);
    if (status != SHELL_STATUS_OK) {
        return status;
    }

    esp_err_t err = hardware_i2c_probe(address, HARDWARE_I2C_DEFAULT_TIMEOUT_MS);
    if (err == ESP_OK) {
        bin_writef(io, "I2C PROBE 0x%02X FOUND\r\nOK\r\n", address);
        return SHELL_STATUS_OK;
    }
    if (err == ESP_ERR_NOT_FOUND) {
        bin_writef(io, "I2C PROBE 0x%02X MISSING\r\nOK\r\n", address);
        return SHELL_STATUS_OK;
    }

    return i2c_io_error(io);
}

static shell_status_t run_scan(char *cursor, const shell_exec_io_t *io)
{
    if (next_token(&cursor) != NULL) {
        return i2c_bad_input(io);
    }

    shell_status_t status = ensure_hardware_ready(io);
    if (status != SHELL_STATUS_OK) {
        return status;
    }

    if (!hardware_i2c_is_configured()) {
        return i2c_io_error(io);
    }

    bin_write(io, "I2C SCAN");
    size_t found = 0;
    for (uint8_t address = I2C_SCAN_FIRST_ADDR; address <= I2C_SCAN_LAST_ADDR;
         address++) {
        esp_err_t err = hardware_i2c_probe(address, I2C_SCAN_TIMEOUT_MS);
        if (err == ESP_OK) {
            bin_writef(io, " 0x%02X", address);
            found++;
        } else if (err != ESP_ERR_NOT_FOUND) {
            bin_write(io, "\r\n");
            return i2c_io_error(io);
        }
    }

    if (found == 0U) {
        bin_write(io, " none");
    }
    bin_write(io, "\r\nOK\r\n");
    return SHELL_STATUS_OK;
}

shell_status_t bin_hardware_i2c_exec(const char *args, const shell_exec_io_t *io)
{
    char buf[256];
    strncpy(buf, skip_ws(args), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *cursor = buf;
    char *action = next_token(&cursor);
    if (!action) {
        return i2c_bad_input(io);
    }

    if (strcasecmp(action, "i2c") == 0) {
        action = next_token(&cursor);
        if (!action) {
            return i2c_bad_input(io);
        }
    }

    if (strcasecmp(action, "config") == 0) {
        return run_config(cursor, io);
    }
    if (strcasecmp(action, "probe") == 0) {
        return run_probe(cursor, io);
    }
    if (strcasecmp(action, "scan") == 0) {
        return run_scan(cursor, io);
    }

    return i2c_bad_input(io);
}
