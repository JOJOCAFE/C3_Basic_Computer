#include "hardware_adc.h"

#include "sdkconfig.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

hardware_adc_read_config_t hardware_adc_default_config(void)
{
    hardware_adc_read_config_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .enable_calibration = true,
        .allow_non_adc1 = false,
    };
    return config;
}

static esp_err_t validate_adc_policy(gpio_num_t gpio_num, adc_unit_t unit,
                                     const hardware_adc_read_config_t *config)
{
    if (!config->allow_non_adc1 && unit != ADC_UNIT_1) {
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_IDF_TARGET_ESP32C3
    if (!config->allow_non_adc1 &&
        (gpio_num < GPIO_NUM_0 || gpio_num > GPIO_NUM_4)) {
        return ESP_ERR_INVALID_ARG;
    }
#else
    (void)gpio_num;
#endif

    return ESP_OK;
}

static bool create_calibration(adc_unit_t unit, adc_channel_t channel,
                               adc_atten_t atten, adc_bitwidth_t bitwidth,
                               adc_cali_handle_t *handle_out)
{
    *handle_out = NULL;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t curve_config = {
        .unit_id = unit,
        .chan = channel,
        .atten = atten,
        .bitwidth = bitwidth,
    };
    if (adc_cali_create_scheme_curve_fitting(&curve_config, handle_out) == ESP_OK) {
        return true;
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t line_config = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = bitwidth,
    };
    if (adc_cali_create_scheme_line_fitting(&line_config, handle_out) == ESP_OK) {
        return true;
    }
#endif

    return false;
}

static void delete_calibration(adc_cali_handle_t handle)
{
    if (handle == NULL) {
        return;
    }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (adc_cali_delete_scheme_curve_fitting(handle) == ESP_OK) {
        return;
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    (void)adc_cali_delete_scheme_line_fitting(handle);
#endif
}

esp_err_t hardware_adc_read_gpio(gpio_num_t gpio_num,
                                 const hardware_adc_read_config_t *config,
                                 hardware_adc_result_t *result_out)
{
    if (result_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    hardware_adc_read_config_t local_config = config != NULL
        ? *config
        : hardware_adc_default_config();

    adc_unit_t unit = ADC_UNIT_1;
    adc_channel_t channel = ADC_CHANNEL_0;
    esp_err_t err = adc_oneshot_io_to_channel(gpio_num, &unit, &channel);
    if (err != ESP_OK) {
        return err;
    }

    err = validate_adc_policy(gpio_num, unit, &local_config);
    if (err != ESP_OK) {
        return err;
    }

    adc_oneshot_unit_handle_t unit_handle = NULL;
    adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    err = adc_oneshot_new_unit(&unit_config, &unit_handle);
    if (err != ESP_OK) {
        return err;
    }

    adc_oneshot_chan_cfg_t channel_config = {
        .atten = local_config.atten,
        .bitwidth = local_config.bitwidth,
    };
    err = adc_oneshot_config_channel(unit_handle, channel, &channel_config);
    if (err != ESP_OK) {
        (void)adc_oneshot_del_unit(unit_handle);
        return err;
    }

    int raw = 0;
    err = adc_oneshot_read(unit_handle, channel, &raw);
    if (err != ESP_OK) {
        (void)adc_oneshot_del_unit(unit_handle);
        return err;
    }

    hardware_adc_result_t result = {
        .raw = raw,
        .millivolts = 0,
        .millivolts_valid = false,
        .unit = unit,
        .channel = channel,
    };

    if (local_config.enable_calibration) {
        adc_cali_handle_t cali_handle = NULL;
        if (create_calibration(unit, channel, local_config.atten,
                               local_config.bitwidth, &cali_handle)) {
            int millivolts = 0;
            if (adc_cali_raw_to_voltage(cali_handle, raw, &millivolts) == ESP_OK) {
                result.millivolts = millivolts;
                result.millivolts_valid = true;
            }
            delete_calibration(cali_handle);
        }
    }

    (void)adc_oneshot_del_unit(unit_handle);
    *result_out = result;
    return ESP_OK;
}
