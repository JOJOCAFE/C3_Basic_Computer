#include "bin_internal.h"

#include "hardware.h"
#include "hardware_gpio.h"

#include <ctype.h>
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

static bool parse_level(const char *text, int *out)
{
    int value = 0;
    if (!parse_int(text, &value) || (value != 0 && value != 1)) {
        return false;
    }
    *out = value;
    return true;
}

static bool parse_pull(const char *text, hardware_gpio_pull_t *out)
{
    if (!text || !out) {
        return false;
    }

    if (strcasecmp(text, "none") == 0 || strcasecmp(text, "floating") == 0) {
        *out = HARDWARE_GPIO_PULL_FLOATING;
    } else if (strcasecmp(text, "up") == 0) {
        *out = HARDWARE_GPIO_PULL_UP;
    } else if (strcasecmp(text, "down") == 0) {
        *out = HARDWARE_GPIO_PULL_DOWN;
    } else if (strcasecmp(text, "updown") == 0 ||
               strcasecmp(text, "up-down") == 0) {
        *out = HARDWARE_GPIO_PULL_UP_DOWN;
    } else {
        return false;
    }

    return true;
}

static shell_status_t gpio_bad_input(const shell_exec_io_t *io)
{
    bin_write(io, "Bad input\r\n");
    return SHELL_STATUS_BAD_INPUT;
}

static shell_status_t gpio_io_error(const shell_exec_io_t *io)
{
    bin_write(io, "GPIO failed\r\n");
    return SHELL_STATUS_IO_ERROR;
}

static shell_status_t run_gpio(char *args, const shell_exec_io_t *io)
{
    char *cursor = args;
    char *action = next_token(&cursor);
    if (!action) {
        return gpio_bad_input(io);
    }

    int gpio = -1;
    int level = 0;
    bool have_level = false;
    bool open_drain = false;
    hardware_gpio_pull_t pull = HARDWARE_GPIO_PULL_FLOATING;

    char *token;
    while ((token = next_token(&cursor)) != NULL) {
        if (strcasecmp(token, "-p") == 0) {
            char *value = next_token(&cursor);
            if (!parse_int(value, &gpio)) {
                return gpio_bad_input(io);
            }
        } else if (strcasecmp(token, "-v") == 0) {
            char *value = next_token(&cursor);
            if (!parse_level(value, &level)) {
                return gpio_bad_input(io);
            }
            have_level = true;
        } else if (strcasecmp(token, "--pull") == 0) {
            char *value = next_token(&cursor);
            if (!parse_pull(value, &pull)) {
                return gpio_bad_input(io);
            }
        } else if (strcasecmp(token, "--open-drain") == 0) {
            open_drain = true;
        } else {
            return gpio_bad_input(io);
        }
    }

    if (gpio < 0 || gpio > 21) {
        return gpio_bad_input(io);
    }

    esp_err_t err = hardware_init();
    if (err != ESP_OK) {
        bin_write(io, "Hardware init failed\r\n");
        return SHELL_STATUS_IO_ERROR;
    }

    if (strcasecmp(action, "in") == 0 || strcasecmp(action, "input") == 0) {
        hardware_gpio_input_config_t config = {
            .pin = (gpio_num_t)gpio,
            .pull = pull,
            .allow_protected_pin = false,
        };
        err = hardware_gpio_configure_input(&config);
        if (err != ESP_OK) {
            return gpio_io_error(io);
        }
        bin_writef(io, "GPIO%d IN\r\nOK\r\n", gpio);
        return SHELL_STATUS_OK;
    }

    if (strcasecmp(action, "out") == 0 || strcasecmp(action, "output") == 0) {
        hardware_gpio_output_config_t config = {
            .pin = (gpio_num_t)gpio,
            .initial_level = have_level ? level : 0,
            .open_drain = open_drain,
            .allow_protected_pin = false,
        };
        err = hardware_gpio_configure_output(&config);
        if (err != ESP_OK) {
            return gpio_io_error(io);
        }
        bin_writef(io, "GPIO%d OUT %d\r\nOK\r\n", gpio, config.initial_level ? 1 : 0);
        return SHELL_STATUS_OK;
    }

    if (strcasecmp(action, "read") == 0) {
        err = hardware_gpio_read((gpio_num_t)gpio, &level, false);
        if (err != ESP_OK) {
            return gpio_io_error(io);
        }
        bin_writef(io, "GPIO%d READ %d\r\nOK\r\n", gpio, level ? 1 : 0);
        return SHELL_STATUS_OK;
    }

    if (strcasecmp(action, "write") == 0 || strcasecmp(action, "set") == 0) {
        if (!have_level) {
            return gpio_bad_input(io);
        }
        err = hardware_gpio_write((gpio_num_t)gpio, level, false);
        if (err != ESP_OK) {
            return gpio_io_error(io);
        }
        bin_writef(io, "GPIO%d WRITE %d\r\nOK\r\n", gpio, level ? 1 : 0);
        return SHELL_STATUS_OK;
    }

    if (strcasecmp(action, "toggle") == 0) {
        err = hardware_gpio_toggle((gpio_num_t)gpio, false);
        if (err != ESP_OK) {
            return gpio_io_error(io);
        }
        err = hardware_gpio_read((gpio_num_t)gpio, &level, false);
        if (err != ESP_OK) {
            return gpio_io_error(io);
        }
        bin_writef(io, "GPIO%d TOGGLE %d\r\nOK\r\n", gpio, level ? 1 : 0);
        return SHELL_STATUS_OK;
    }

    return gpio_bad_input(io);
}

shell_status_t bin_hardware_exec(const char *args, const shell_exec_io_t *io)
{
    char buf[256];
    strncpy(buf, skip_ws(args), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *cursor = buf;
    char *subcommand = next_token(&cursor);
    if (!subcommand) {
        bin_write(io, "Bad input\r\n");
        return SHELL_STATUS_BAD_INPUT;
    }

    if (strcasecmp(subcommand, "gpio") == 0) {
        return run_gpio(cursor, io);
    }
    if (strcasecmp(subcommand, "adc") == 0) {
        return bin_hardware_adc_exec(cursor, io);
    }
    if (strcasecmp(subcommand, "i2c") == 0) {
        return bin_hardware_i2c_exec(cursor, io);
    }
    if (strcasecmp(subcommand, "spi") == 0) {
        return bin_hardware_spi_exec(cursor, io);
    }

    bin_write(io, "Bad input\r\n");
    return SHELL_STATUS_BAD_INPUT;
}
