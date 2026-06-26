#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HARDWARE_PROTECTED_GPIO18 18
#define HARDWARE_PROTECTED_GPIO19 19

typedef struct {
    bool initialized;
    uint32_t protected_pin_mask;
} hardware_status_t;

esp_err_t hardware_init(void);
esp_err_t hardware_reset(void);
hardware_status_t hardware_get_status(void);

bool hardware_is_protected_pin(int gpio_num);
uint32_t hardware_get_protected_pin_mask(void);

#ifdef __cplusplus
}
#endif
