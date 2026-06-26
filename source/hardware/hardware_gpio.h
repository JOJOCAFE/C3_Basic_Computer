#pragma once

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HARDWARE_GPIO_PULL_FLOATING = 0,
    HARDWARE_GPIO_PULL_UP,
    HARDWARE_GPIO_PULL_DOWN,
    HARDWARE_GPIO_PULL_UP_DOWN,
} hardware_gpio_pull_t;

typedef struct {
    gpio_num_t pin;
    hardware_gpio_pull_t pull;
    bool allow_protected_pin;
} hardware_gpio_input_config_t;

typedef struct {
    gpio_num_t pin;
    int initial_level;
    bool open_drain;
    bool allow_protected_pin;
} hardware_gpio_output_config_t;

bool hardware_gpio_is_valid_pin(gpio_num_t pin);
bool hardware_gpio_is_valid_output_pin(gpio_num_t pin);
bool hardware_gpio_is_protected_pin(gpio_num_t pin);

esp_err_t hardware_gpio_configure_input(const hardware_gpio_input_config_t *config);
esp_err_t hardware_gpio_configure_output(const hardware_gpio_output_config_t *config);
esp_err_t hardware_gpio_set_pull(gpio_num_t pin, hardware_gpio_pull_t pull,
                                 bool allow_protected_pin);
esp_err_t hardware_gpio_read(gpio_num_t pin, int *level_out,
                             bool allow_protected_pin);
esp_err_t hardware_gpio_write(gpio_num_t pin, int level,
                              bool allow_protected_pin);
esp_err_t hardware_gpio_toggle(gpio_num_t pin, bool allow_protected_pin);

#ifdef __cplusplus
}
#endif
//Keep Going.
