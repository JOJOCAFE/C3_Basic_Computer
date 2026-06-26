#include "bin_internal.h"

#include "hardware.h"
#include "hardware_spi.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define BIN_SPI_MAX_XFER_BYTES 256U

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

static bool parse_int(const char *text, int *out)
{
    if (!text || !out) {
        return false;
    }

    char *end = NULL;
    long value = strtol(text, &end, 0);
    if (end == text || *end != '\0') {
        return false;
    }
    *out = (int)value;
    return true;
}

static shell_status_t spi_bad_input(const shell_exec_io_t *io)
{
    bin_write(io, "Bad input\r\n");
    return SHELL_STATUS_BAD_INPUT;
}

static shell_status_t spi_io_error(const shell_exec_io_t *io)
{
    bin_write(io, "SPI failed\r\n");
    return SHELL_STATUS_IO_ERROR;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static bool parse_hexbytes(const char *text, uint8_t *out, size_t out_cap,
                           size_t *out_len)
{
    if (!text || !out || !out_len) {
        return false;
    }

    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text += 2;
    }

    size_t nibbles = strlen(text);
    if (nibbles == 0 || (nibbles % 2U) != 0U) {
        return false;
    }

    size_t len = nibbles / 2U;
    if (len == 0 || len > out_cap) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        int high = hex_value(text[i * 2U]);
        int low = hex_value(text[i * 2U + 1U]);
        if (high < 0 || low < 0) {
            return false;
        }
        out[i] = (uint8_t)((high << 4) | low);
    }

    *out_len = len;
    return true;
}

static void write_hexbytes(const shell_exec_io_t *io, const uint8_t *data,
                           size_t len)
{
    static const char digits[] = "0123456789ABCDEF";
    char line[(BIN_SPI_MAX_XFER_BYTES * 2U) + 10U];
    size_t pos = 0;

    memcpy(line, "SPI RX ", 7);
    pos = 7;
    for (size_t i = 0; i < len; i++) {
        line[pos++] = digits[(data[i] >> 4) & 0x0F];
        line[pos++] = digits[data[i] & 0x0F];
    }
    line[pos++] = '\r';
    line[pos++] = '\n';
    line[pos] = '\0';

    bin_write(io, line);
}

static shell_status_t run_config(char *args, const shell_exec_io_t *io)
{
    int mosi = -1;
    int miso = -1;
    int sclk = -1;
    int cs = GPIO_NUM_NC;
    int frequency = HARDWARE_SPI_DEFAULT_FREQUENCY_HZ;
    int mode = 0;

    char *cursor = args;
    char *token;
    while ((token = next_token(&cursor)) != NULL) {
        int *target = NULL;

        if (strcasecmp(token, "-mosi") == 0) {
            target = &mosi;
        } else if (strcasecmp(token, "-miso") == 0) {
            target = &miso;
        } else if (strcasecmp(token, "-sclk") == 0) {
            target = &sclk;
        } else if (strcasecmp(token, "-cs") == 0) {
            target = &cs;
        } else if (strcasecmp(token, "-f") == 0) {
            target = &frequency;
        } else if (strcasecmp(token, "-m") == 0) {
            target = &mode;
        } else {
            return spi_bad_input(io);
        }

        char *value = next_token(&cursor);
        if (!parse_int(value, target)) {
            return spi_bad_input(io);
        }
    }

    if (mosi < 0 || miso < 0 || sclk < 0 || frequency <= 0 ||
        mode < 0 || mode > 3) {
        return spi_bad_input(io);
    }

    esp_err_t err = hardware_init();
    if (err != ESP_OK) {
        bin_write(io, "Hardware init failed\r\n");
        return SHELL_STATUS_IO_ERROR;
    }

    hardware_spi_config_t config = hardware_spi_default_config(
        (gpio_num_t)mosi, (gpio_num_t)miso, (gpio_num_t)sclk, (gpio_num_t)cs);
    config.frequency_hz = frequency;
    config.mode = (uint8_t)mode;
    config.allow_protected_pins = false;

    err = hardware_spi_configure(&config);
    if (err != ESP_OK) {
        return spi_io_error(io);
    }

    if (cs == GPIO_NUM_NC) {
        bin_writef(io, "SPI CONFIG MOSI %d MISO %d SCLK %d F %d MODE %d\r\nOK\r\n",
                   mosi, miso, sclk, frequency, mode);
    } else {
        bin_writef(io,
                   "SPI CONFIG MOSI %d MISO %d SCLK %d CS %d F %d MODE %d\r\nOK\r\n",
                   mosi, miso, sclk, cs, frequency, mode);
    }
    return SHELL_STATUS_OK;
}

static shell_status_t run_xfer(char *args, const shell_exec_io_t *io)
{
    uint8_t tx[BIN_SPI_MAX_XFER_BYTES];
    uint8_t rx[BIN_SPI_MAX_XFER_BYTES];
    size_t len = 0;
    bool have_tx = false;

    char *cursor = args;
    char *token;
    while ((token = next_token(&cursor)) != NULL) {
        if (strcasecmp(token, "-tx") != 0) {
            return spi_bad_input(io);
        }

        char *value = next_token(&cursor);
        if (have_tx || !parse_hexbytes(value, tx, sizeof(tx), &len)) {
            return spi_bad_input(io);
        }
        have_tx = true;
    }

    if (!have_tx) {
        return spi_bad_input(io);
    }

    esp_err_t err = hardware_spi_transfer(tx, rx, len);
    if (err != ESP_OK) {
        return spi_io_error(io);
    }

    write_hexbytes(io, rx, len);
    bin_write(io, "OK\r\n");
    return SHELL_STATUS_OK;
}

shell_status_t bin_hardware_spi_exec(const char *args, const shell_exec_io_t *io)
{
    char buf[256];
    strncpy(buf, skip_ws(args), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *cursor = buf;
    char *action = next_token(&cursor);
    if (!action) {
        return spi_bad_input(io);
    }

    if (strcasecmp(action, "config") == 0) {
        return run_config(cursor, io);
    }

    if (strcasecmp(action, "xfer") == 0) {
        return run_xfer(cursor, io);
    }

    return spi_bad_input(io);
}
