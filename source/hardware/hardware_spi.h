#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/spi_master.h"
#include "esp_err.h"
#include "hal/gpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HARDWARE_SPI_DEFAULT_FREQUENCY_HZ 1000000
#define HARDWARE_SPI_MAX_TRANSFER_BYTES 4092U

typedef struct {
    gpio_num_t mosi_pin;
    gpio_num_t miso_pin;
    gpio_num_t sclk_pin;
    gpio_num_t cs_pin;
    int frequency_hz;
    uint8_t mode;
    int queue_size;
    bool allow_protected_pins;
} hardware_spi_config_t;

hardware_spi_config_t hardware_spi_default_config(gpio_num_t mosi_pin,
                                                  gpio_num_t miso_pin,
                                                  gpio_num_t sclk_pin,
                                                  gpio_num_t cs_pin);

esp_err_t hardware_spi_configure(const hardware_spi_config_t *config);
esp_err_t hardware_spi_deinit(void);
bool hardware_spi_is_configured(void);
esp_err_t hardware_spi_transfer(const uint8_t *tx_data, uint8_t *rx_data,
                                size_t data_len);

#ifdef __cplusplus
}
#endif
//Keep Going.
