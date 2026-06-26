#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "hal/gpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HARDWARE_I2C_DEFAULT_FREQUENCY_HZ 100000U
#define HARDWARE_I2C_DEFAULT_TIMEOUT_MS 1000
#define HARDWARE_I2C_MAX_TRANSFER_BYTES 128U

typedef struct {
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
    uint32_t frequency_hz;
    bool enable_internal_pullups;
    bool allow_protected_pins;
} hardware_i2c_config_t;

hardware_i2c_config_t hardware_i2c_default_config(gpio_num_t sda_pin,
                                                  gpio_num_t scl_pin);

esp_err_t hardware_i2c_configure(const hardware_i2c_config_t *config);
esp_err_t hardware_i2c_deinit(void);
bool hardware_i2c_is_configured(void);

esp_err_t hardware_i2c_probe(uint8_t address, int timeout_ms);
esp_err_t hardware_i2c_write(uint8_t address, const uint8_t *data,
                             size_t data_len, int timeout_ms);
esp_err_t hardware_i2c_write_read(uint8_t address,
                                  const uint8_t *write_data,
                                  size_t write_len,
                                  uint8_t *read_data,
                                  size_t read_len,
                                  int timeout_ms);
esp_err_t hardware_i2c_read_register(uint8_t address, uint8_t reg,
                                     uint8_t *data, size_t data_len,
                                     int timeout_ms);
esp_err_t hardware_i2c_write_register(uint8_t address, uint8_t reg,
                                      const uint8_t *data, size_t data_len,
                                      int timeout_ms);

#ifdef __cplusplus
}
#endif
