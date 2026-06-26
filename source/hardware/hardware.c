#include "hardware.h"

#include "hardware_i2c.h"
#include "hardware_spi.h"

static bool s_hardware_initialized;

esp_err_t hardware_init(void)
{
    s_hardware_initialized = true;
    return ESP_OK;
}

esp_err_t hardware_reset(void)
{
    esp_err_t err = hardware_i2c_deinit();
    if (err != ESP_OK) {
        return err;
    }

    err = hardware_spi_deinit();
    if (err != ESP_OK) {
        return err;
    }

    s_hardware_initialized = false;
    return ESP_OK;
}

hardware_status_t hardware_get_status(void)
{
    hardware_status_t status = {
        .initialized = s_hardware_initialized,
        .protected_pin_mask = hardware_get_protected_pin_mask(),
    };
    return status;
}

bool hardware_is_protected_pin(int gpio_num)
{
    return gpio_num == HARDWARE_PROTECTED_GPIO18 ||
           gpio_num == HARDWARE_PROTECTED_GPIO19;
}

uint32_t hardware_get_protected_pin_mask(void)
{
    return (1UL << HARDWARE_PROTECTED_GPIO18) |
           (1UL << HARDWARE_PROTECTED_GPIO19);
}
//Keep Going.
