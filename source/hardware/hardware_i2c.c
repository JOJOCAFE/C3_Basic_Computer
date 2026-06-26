#include "hardware_i2c.h"

#include <string.h>

#include "driver/gpio.h"
#include "hardware.h"
#include "sdkconfig.h"

static i2c_master_bus_handle_t s_i2c_bus;
static uint32_t s_i2c_frequency_hz = HARDWARE_I2C_DEFAULT_FREQUENCY_HZ;

hardware_i2c_config_t hardware_i2c_default_config(gpio_num_t sda_pin,
                                                  gpio_num_t scl_pin)
{
    hardware_i2c_config_t config = {
        .sda_pin = sda_pin,
        .scl_pin = scl_pin,
        .frequency_hz = HARDWARE_I2C_DEFAULT_FREQUENCY_HZ,
        .enable_internal_pullups = true,
        .allow_protected_pins = false,
    };
    return config;
}

static esp_err_t validate_pin(gpio_num_t pin, bool allow_protected_pins)
{
    if (!GPIO_IS_VALID_GPIO(pin)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!allow_protected_pins && hardware_is_protected_pin(pin)) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t validate_address(uint8_t address)
{
    return address <= 0x7f ? ESP_OK : ESP_ERR_INVALID_ARG;
}

static int normalize_timeout(int timeout_ms)
{
    return timeout_ms > 0 ? timeout_ms : HARDWARE_I2C_DEFAULT_TIMEOUT_MS;
}

static esp_err_t add_device(uint8_t address,
                            i2c_master_dev_handle_t *device_out)
{
    esp_err_t err = validate_address(address);
    if (err != ESP_OK) {
        return err;
    }

    if (s_i2c_bus == NULL || device_out == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = s_i2c_frequency_hz,
    };

    return i2c_master_bus_add_device(s_i2c_bus, &device_config, device_out);
}

esp_err_t hardware_i2c_configure(const hardware_i2c_config_t *config)
{
    if (config == NULL || config->frequency_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = validate_pin(config->sda_pin, config->allow_protected_pins);
    if (err != ESP_OK) {
        return err;
    }

    err = validate_pin(config->scl_pin, config->allow_protected_pins);
    if (err != ESP_OK) {
        return err;
    }

    (void)hardware_i2c_deinit();

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = config->sda_pin,
        .scl_io_num = config->scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = config->enable_internal_pullups,
    };

    err = i2c_new_master_bus(&bus_config, &s_i2c_bus);
    if (err != ESP_OK) {
        s_i2c_bus = NULL;
        return err;
    }

    s_i2c_frequency_hz = config->frequency_hz;
    return ESP_OK;
}

esp_err_t hardware_i2c_deinit(void)
{
    if (s_i2c_bus == NULL) {
        return ESP_OK;
    }

    esp_err_t err = i2c_del_master_bus(s_i2c_bus);
    if (err == ESP_OK) {
        s_i2c_bus = NULL;
        s_i2c_frequency_hz = HARDWARE_I2C_DEFAULT_FREQUENCY_HZ;
    }
    return err;
}

bool hardware_i2c_is_configured(void)
{
    return s_i2c_bus != NULL;
}

esp_err_t hardware_i2c_probe(uint8_t address, int timeout_ms)
{
    esp_err_t err = validate_address(address);
    if (err != ESP_OK) {
        return err;
    }

    if (s_i2c_bus == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return i2c_master_probe(s_i2c_bus, address, normalize_timeout(timeout_ms));
}

esp_err_t hardware_i2c_write(uint8_t address, const uint8_t *data,
                             size_t data_len, int timeout_ms)
{
    if (data == NULL || data_len == 0 ||
        data_len > HARDWARE_I2C_MAX_TRANSFER_BYTES) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_master_dev_handle_t device = NULL;
    esp_err_t err = add_device(address, &device);
    if (err != ESP_OK) {
        return err;
    }

    err = i2c_master_transmit(device, data, data_len,
                              normalize_timeout(timeout_ms));
    (void)i2c_master_bus_rm_device(device);
    return err;
}

esp_err_t hardware_i2c_write_read(uint8_t address,
                                  const uint8_t *write_data,
                                  size_t write_len,
                                  uint8_t *read_data,
                                  size_t read_len,
                                  int timeout_ms)
{
    if (write_data == NULL || read_data == NULL ||
        write_len == 0 || read_len == 0 ||
        write_len > HARDWARE_I2C_MAX_TRANSFER_BYTES ||
        read_len > HARDWARE_I2C_MAX_TRANSFER_BYTES) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_master_dev_handle_t device = NULL;
    esp_err_t err = add_device(address, &device);
    if (err != ESP_OK) {
        return err;
    }

    err = i2c_master_transmit_receive(device, write_data, write_len,
                                      read_data, read_len,
                                      normalize_timeout(timeout_ms));
    (void)i2c_master_bus_rm_device(device);
    return err;
}

esp_err_t hardware_i2c_read_register(uint8_t address, uint8_t reg,
                                     uint8_t *data, size_t data_len,
                                     int timeout_ms)
{
    return hardware_i2c_write_read(address, &reg, sizeof(reg), data, data_len,
                                   timeout_ms);
}

esp_err_t hardware_i2c_write_register(uint8_t address, uint8_t reg,
                                      const uint8_t *data, size_t data_len,
                                      int timeout_ms)
{
    if (data_len + 1U > HARDWARE_I2C_MAX_TRANSFER_BYTES ||
        (data == NULL && data_len > 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[HARDWARE_I2C_MAX_TRANSFER_BYTES] = {0};
    buffer[0] = reg;
    if (data_len > 0) {
        memcpy(&buffer[1], data, data_len);
    }

    return hardware_i2c_write(address, buffer, data_len + 1U, timeout_ms);
}
