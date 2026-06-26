#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

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
basic_hw_result_t basic_hw_aread(int gpio, int *raw_out);

#ifdef __cplusplus
}
#endif
//Keep Going.
