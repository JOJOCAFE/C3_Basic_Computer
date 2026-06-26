#include "bin_internal.h"

#include "hardware.h"
#include "hardware_adc.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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

static bool parse_gpio(const char *text, gpio_num_t *out)
{
    if (!text || !out) {
        return false;
    }

    errno = 0;
    char *end = NULL;
    long value = strtol(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || value < 0 || value > 21) {
        return false;
    }

    *out = (gpio_num_t)value;
    return true;
}

static shell_status_t adc_bad_input(const shell_exec_io_t *io)
{
    bin_write(io, "Bad input\r\n");
    return SHELL_STATUS_BAD_INPUT;
}

static shell_status_t adc_io_error(const shell_exec_io_t *io)
{
    bin_write(io, "ADC failed\r\n");
    return SHELL_STATUS_IO_ERROR;
}

static shell_status_t run_read(char *cursor, const shell_exec_io_t *io)
{
    gpio_num_t gpio = GPIO_NUM_NC;
    bool have_gpio = false;

    char *token = NULL;
    while ((token = next_token(&cursor)) != NULL) {
        if (strcasecmp(token, "-p") != 0) {
            return adc_bad_input(io);
        }

        if (have_gpio) {
            return adc_bad_input(io);
        }

        char *value = next_token(&cursor);
        if (!parse_gpio(value, &gpio)) {
            return adc_bad_input(io);
        }
        have_gpio = true;
    }

    if (!have_gpio) {
        return adc_bad_input(io);
    }

    esp_err_t err = hardware_init();
    if (err != ESP_OK) {
        bin_write(io, "Hardware init failed\r\n");
        return SHELL_STATUS_IO_ERROR;
    }

    hardware_adc_result_t result = {0};
    err = hardware_adc_read_gpio(gpio, NULL, &result);
    if (err != ESP_OK) {
        return adc_io_error(io);
    }

    bin_writef(io, "ADC GPIO%d RAW %d", (int)gpio, result.raw);
    if (result.millivolts_valid) {
        bin_writef(io, " MV %d", result.millivolts);
    }
    bin_write(io, "\r\nOK\r\n");
    return SHELL_STATUS_OK;
}

shell_status_t bin_hardware_adc_exec(const char *args, const shell_exec_io_t *io)
{
    if (!args) {
        return adc_bad_input(io);
    }

    char buf[256];
    const char *trimmed = skip_ws(args);
    if (strlen(trimmed) >= sizeof(buf)) {
        return adc_bad_input(io);
    }
    strncpy(buf, trimmed, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *cursor = buf;
    char *action = next_token(&cursor);
    if (!action) {
        return adc_bad_input(io);
    }

    if (strcasecmp(action, "read") == 0) {
        return run_read(cursor, io);
    }

    return adc_bad_input(io);
}
//Keep Going.
