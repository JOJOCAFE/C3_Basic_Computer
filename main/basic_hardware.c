#include "basic_hardware.h"

#include "hardware.h"
#include "hardware_adc.h"
#include "hardware_gpio.h"

static basic_hw_result_t hw_result(basic_hw_status_t status, esp_err_t driver_error,
                                   const char *message)
{
    basic_hw_result_t result = {
        .status = status,
        .driver_error = driver_error,
        .message = message,
    };
    return result;
}

static basic_hw_result_t map_driver_error(int pin, bool output, esp_err_t err)
{
    if (err == ESP_OK) {
        return hw_result(BASIC_HW_OK, ESP_OK, "OK");
    }
    if (pin < 0 || pin > 21) {
        return hw_result(BASIC_HW_ERR_BAD_ARGUMENT, err, "bad hardware argument");
    }
    if (hardware_gpio_is_protected_pin((gpio_num_t)pin)) {
        return hw_result(BASIC_HW_ERR_PROTECTED_PIN, err, "protected pin");
    }
    if ((output && !hardware_gpio_is_valid_output_pin((gpio_num_t)pin)) ||
        (!output && !hardware_gpio_is_valid_pin((gpio_num_t)pin))) {
        return hw_result(BASIC_HW_ERR_UNSUPPORTED_PIN, err, "unsupported pin");
    }
    if (err == ESP_ERR_INVALID_STATE) {
        return hw_result(BASIC_HW_ERR_NOT_CONFIGURED, err, "hardware not configured");
    }
    if (err == ESP_ERR_INVALID_ARG) {
        return hw_result(BASIC_HW_ERR_BAD_ARGUMENT, err, "bad hardware argument");
    }
    return hw_result(BASIC_HW_ERR_DRIVER, err, "hardware driver failed");
}

basic_hw_result_t basic_hw_init(void)
{
    esp_err_t err = hardware_init();
    if (err != ESP_OK) {
        return hw_result(BASIC_HW_ERR_DRIVER, err, "hardware driver failed");
    }
    return hw_result(BASIC_HW_OK, ESP_OK, "OK");
}

basic_hw_result_t basic_hw_pinmode(int pin, basic_hw_pin_mode_t mode)
{
    basic_hw_result_t init = basic_hw_init();
    if (init.status != BASIC_HW_OK) {
        return init;
    }

    switch (mode) {
    case BASIC_HW_PIN_INPUT: {
        hardware_gpio_input_config_t config = {
            .pin = (gpio_num_t)pin,
            .pull = HARDWARE_GPIO_PULL_FLOATING,
            .allow_protected_pin = false,
        };
        return map_driver_error(pin, false, hardware_gpio_configure_input(&config));
    }
    case BASIC_HW_PIN_INPUT_PULLUP: {
        hardware_gpio_input_config_t config = {
            .pin = (gpio_num_t)pin,
            .pull = HARDWARE_GPIO_PULL_UP,
            .allow_protected_pin = false,
        };
        return map_driver_error(pin, false, hardware_gpio_configure_input(&config));
    }
    case BASIC_HW_PIN_INPUT_PULLDOWN: {
        hardware_gpio_input_config_t config = {
            .pin = (gpio_num_t)pin,
            .pull = HARDWARE_GPIO_PULL_DOWN,
            .allow_protected_pin = false,
        };
        return map_driver_error(pin, false, hardware_gpio_configure_input(&config));
    }
    case BASIC_HW_PIN_OUTPUT:
    case BASIC_HW_PIN_OUTPUT_OPEN_DRAIN: {
        hardware_gpio_output_config_t config = {
            .pin = (gpio_num_t)pin,
            .initial_level = 0,
            .open_drain = mode == BASIC_HW_PIN_OUTPUT_OPEN_DRAIN,
            .allow_protected_pin = false,
        };
        return map_driver_error(pin, true, hardware_gpio_configure_output(&config));
    }
    default:
        return hw_result(BASIC_HW_ERR_BAD_ARGUMENT, ESP_ERR_INVALID_ARG,
                         "bad hardware argument");
    }
}

basic_hw_result_t basic_hw_dwrite(int pin, int level)
{
    if (level != 0 && level != 1) {
        return hw_result(BASIC_HW_ERR_BAD_ARGUMENT, ESP_ERR_INVALID_ARG,
                         "bad hardware argument");
    }

    basic_hw_result_t init = basic_hw_init();
    if (init.status != BASIC_HW_OK) {
        return init;
    }
    return map_driver_error(pin, true,
                            hardware_gpio_write((gpio_num_t)pin, level, false));
}

basic_hw_result_t basic_hw_dread(int pin, int *level_out)
{
    if (!level_out) {
        return hw_result(BASIC_HW_ERR_BAD_ARGUMENT, ESP_ERR_INVALID_ARG,
                         "bad hardware argument");
    }

    basic_hw_result_t init = basic_hw_init();
    if (init.status != BASIC_HW_OK) {
        return init;
    }
    return map_driver_error(pin, false,
                            hardware_gpio_read((gpio_num_t)pin, level_out, false));
}

basic_hw_result_t basic_hw_toggle(int pin)
{
    basic_hw_result_t init = basic_hw_init();
    if (init.status != BASIC_HW_OK) {
        return init;
    }
    return map_driver_error(pin, true, hardware_gpio_toggle((gpio_num_t)pin, false));
}

basic_hw_result_t basic_hw_aread(int gpio, int *raw_out)
{
    if (!raw_out) {
        return hw_result(BASIC_HW_ERR_BAD_ARGUMENT, ESP_ERR_INVALID_ARG,
                         "bad hardware argument");
    }

    basic_hw_result_t init = basic_hw_init();
    if (init.status != BASIC_HW_OK) {
        return init;
    }

    hardware_adc_result_t result = {0};
    esp_err_t err = hardware_adc_read_gpio((gpio_num_t)gpio, NULL, &result);
    if (err != ESP_OK) {
        if (gpio < 0 || gpio > 21) {
            return hw_result(BASIC_HW_ERR_BAD_ARGUMENT, err, "bad hardware argument");
        }
        if (hardware_gpio_is_protected_pin((gpio_num_t)gpio)) {
            return hw_result(BASIC_HW_ERR_PROTECTED_PIN, err, "protected pin");
        }
        return hw_result(BASIC_HW_ERR_UNSUPPORTED_PIN, err, "unsupported pin");
    }

    *raw_out = result.raw;
    return hw_result(BASIC_HW_OK, ESP_OK, "OK");
}
