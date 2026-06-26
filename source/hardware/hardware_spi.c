#include "hardware_spi.h"

#include <string.h>

#include "driver/gpio.h"
#include "hardware.h"
#include "sdkconfig.h"

static spi_device_handle_t s_spi_device;
static bool s_spi_bus_initialized;

hardware_spi_config_t hardware_spi_default_config(gpio_num_t mosi_pin,
                                                  gpio_num_t miso_pin,
                                                  gpio_num_t sclk_pin,
                                                  gpio_num_t cs_pin)
{
    hardware_spi_config_t config = {
        .mosi_pin = mosi_pin,
        .miso_pin = miso_pin,
        .sclk_pin = sclk_pin,
        .cs_pin = cs_pin,
        .frequency_hz = HARDWARE_SPI_DEFAULT_FREQUENCY_HZ,
        .mode = 0,
        .queue_size = 1,
        .allow_protected_pins = false,
    };
    return config;
}

static esp_err_t validate_spi_pin(gpio_num_t pin, bool output,
                                  bool optional,
                                  bool allow_protected_pins)
{
    if (optional && pin == GPIO_NUM_NC) {
        return ESP_OK;
    }

    if (output) {
        if (!GPIO_IS_VALID_OUTPUT_GPIO(pin)) {
            return ESP_ERR_INVALID_ARG;
        }
    } else if (!GPIO_IS_VALID_GPIO(pin)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!allow_protected_pins && hardware_is_protected_pin(pin)) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

esp_err_t hardware_spi_configure(const hardware_spi_config_t *config)
{
    if (config == NULL || config->frequency_hz <= 0 ||
        config->mode > 3 || config->queue_size <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = validate_spi_pin(config->mosi_pin, true, false,
                                     config->allow_protected_pins);
    if (err != ESP_OK) {
        return err;
    }

    err = validate_spi_pin(config->miso_pin, false, false,
                           config->allow_protected_pins);
    if (err != ESP_OK) {
        return err;
    }

    err = validate_spi_pin(config->sclk_pin, true, false,
                           config->allow_protected_pins);
    if (err != ESP_OK) {
        return err;
    }

    err = validate_spi_pin(config->cs_pin, true, true,
                           config->allow_protected_pins);
    if (err != ESP_OK) {
        return err;
    }

    (void)hardware_spi_deinit();

    spi_bus_config_t bus_config = {
        .mosi_io_num = config->mosi_pin,
        .miso_io_num = config->miso_pin,
        .sclk_io_num = config->sclk_pin,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .data4_io_num = GPIO_NUM_NC,
        .data5_io_num = GPIO_NUM_NC,
        .data6_io_num = GPIO_NUM_NC,
        .data7_io_num = GPIO_NUM_NC,
        .max_transfer_sz = HARDWARE_SPI_MAX_TRANSFER_BYTES,
        .flags = SPICOMMON_BUSFLAG_MASTER,
    };

    err = spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        return err;
    }
    s_spi_bus_initialized = true;

    spi_device_interface_config_t device_config = {
        .mode = config->mode,
        .clock_speed_hz = config->frequency_hz,
        .spics_io_num = config->cs_pin,
        .queue_size = config->queue_size,
    };

    err = spi_bus_add_device(SPI2_HOST, &device_config, &s_spi_device);
    if (err != ESP_OK) {
        (void)spi_bus_free(SPI2_HOST);
        s_spi_bus_initialized = false;
        s_spi_device = NULL;
        return err;
    }

    return ESP_OK;
}

esp_err_t hardware_spi_deinit(void)
{
    esp_err_t first_err = ESP_OK;

    if (s_spi_device != NULL) {
        first_err = spi_bus_remove_device(s_spi_device);
        if (first_err == ESP_OK) {
            s_spi_device = NULL;
        }
    }

    if (s_spi_bus_initialized) {
        esp_err_t err = spi_bus_free(SPI2_HOST);
        if (first_err == ESP_OK) {
            first_err = err;
        }
        if (err == ESP_OK) {
            s_spi_bus_initialized = false;
        }
    }

    return first_err;
}

bool hardware_spi_is_configured(void)
{
    return s_spi_device != NULL;
}

esp_err_t hardware_spi_transfer(const uint8_t *tx_data, uint8_t *rx_data,
                                size_t data_len)
{
    if (s_spi_device == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (data_len == 0 || data_len > HARDWARE_SPI_MAX_TRANSFER_BYTES ||
        (tx_data == NULL && rx_data == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    spi_transaction_t transaction;
    memset(&transaction, 0, sizeof(transaction));
    transaction.length = data_len * 8U;
    transaction.rxlength = data_len * 8U;
    transaction.tx_buffer = tx_data;
    transaction.rx_buffer = rx_data;

    return spi_device_transmit(s_spi_device, &transaction);
}
