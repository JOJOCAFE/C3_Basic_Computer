#pragma once

#include <stdbool.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "hal/gpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    adc_atten_t atten;
    adc_bitwidth_t bitwidth;
    bool enable_calibration;
    bool allow_non_adc1;
} hardware_adc_read_config_t;

typedef struct {
    int raw;
    int millivolts;
    bool millivolts_valid;
    adc_unit_t unit;
    adc_channel_t channel;
} hardware_adc_result_t;

hardware_adc_read_config_t hardware_adc_default_config(void);
esp_err_t hardware_adc_read_gpio(gpio_num_t gpio_num,
                                 const hardware_adc_read_config_t *config,
                                 hardware_adc_result_t *result_out);

#ifdef __cplusplus
}
#endif
//Keep Going.
