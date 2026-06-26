#include "hardware_gpio.h"

#include "hardware.h"

static esp_err_t validate_gpio_pin(gpio_num_t pin, bool output,
                                   bool allow_protected_pin)
{
    if (output) {
        if (!GPIO_IS_VALID_OUTPUT_GPIO(pin)) {
            return ESP_ERR_INVALID_ARG;
        }
    } else if (!GPIO_IS_VALID_GPIO(pin)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!allow_protected_pin && hardware_is_protected_pin(pin)) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t apply_pull(gpio_num_t pin, hardware_gpio_pull_t pull)
{
    switch (pull) {
    case HARDWARE_GPIO_PULL_FLOATING:
        return gpio_set_pull_mode(pin, GPIO_FLOATING);
    case HARDWARE_GPIO_PULL_UP:
        return gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
    case HARDWARE_GPIO_PULL_DOWN:
        return gpio_set_pull_mode(pin, GPIO_PULLDOWN_ONLY);
    case HARDWARE_GPIO_PULL_UP_DOWN:
        return gpio_set_pull_mode(pin, GPIO_PULLUP_PULLDOWN);
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

bool hardware_gpio_is_valid_pin(gpio_num_t pin)
{
    return GPIO_IS_VALID_GPIO(pin);
}

bool hardware_gpio_is_valid_output_pin(gpio_num_t pin)
{
    return GPIO_IS_VALID_OUTPUT_GPIO(pin);
}

bool hardware_gpio_is_protected_pin(gpio_num_t pin)
{
    return hardware_is_protected_pin(pin);
}

esp_err_t hardware_gpio_configure_input(const hardware_gpio_input_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = validate_gpio_pin(config->pin, false,
                                      config->allow_protected_pin);
    if (err != ESP_OK) {
        return err;
    }

    gpio_config_t io_config = {
        .pin_bit_mask = 1ULL << config->pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    err = gpio_config(&io_config);
    if (err != ESP_OK) {
        return err;
    }

    return apply_pull(config->pin, config->pull);
}

esp_err_t hardware_gpio_configure_output(const hardware_gpio_output_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = validate_gpio_pin(config->pin, true,
                                      config->allow_protected_pin);
    if (err != ESP_OK) {
        return err;
    }

    gpio_config_t io_config = {
        .pin_bit_mask = 1ULL << config->pin,
        .mode = config->open_drain ? GPIO_MODE_OUTPUT_OD : GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    err = gpio_config(&io_config);
    if (err != ESP_OK) {
        return err;
    }

    return gpio_set_level(config->pin, config->initial_level ? 1 : 0);
}

esp_err_t hardware_gpio_set_pull(gpio_num_t pin, hardware_gpio_pull_t pull,
                                 bool allow_protected_pin)
{
    esp_err_t err = validate_gpio_pin(pin, false, allow_protected_pin);
    if (err != ESP_OK) {
        return err;
    }

    return apply_pull(pin, pull);
}

esp_err_t hardware_gpio_read(gpio_num_t pin, int *level_out,
                             bool allow_protected_pin)
{
    if (level_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = validate_gpio_pin(pin, false, allow_protected_pin);
    if (err != ESP_OK) {
        return err;
    }

    *level_out = gpio_get_level(pin);
    return ESP_OK;
}

esp_err_t hardware_gpio_write(gpio_num_t pin, int level,
                              bool allow_protected_pin)
{
    esp_err_t err = validate_gpio_pin(pin, true, allow_protected_pin);
    if (err != ESP_OK) {
        return err;
    }

    return gpio_set_level(pin, level ? 1 : 0);
}

esp_err_t hardware_gpio_toggle(gpio_num_t pin, bool allow_protected_pin)
{
    int level = 0;
    esp_err_t err = hardware_gpio_read(pin, &level, allow_protected_pin);
    if (err != ESP_OK) {
        return err;
    }

    return hardware_gpio_write(pin, level ? 0 : 1, allow_protected_pin);
}
//Keep Going.
