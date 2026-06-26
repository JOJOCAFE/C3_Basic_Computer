#include "input.h"

#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"

#include <string.h>

esp_err_t input_init(void)
{
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    return usb_serial_jtag_driver_install(&cfg);
}

int input_read_line(char *buf, size_t len)
{
    if (!buf || len == 0) {
        return 0;
    }

    size_t idx = 0;
    bool overflow = false;

    while (1) {
        char ch;
        TickType_t wait = overflow ? pdMS_TO_TICKS(50) : portMAX_DELAY;
        int n = usb_serial_jtag_read_bytes(&ch, 1, wait);
        if (n <= 0) {
            if (overflow) {
                usb_serial_jtag_write_bytes("\r\n", 2, portMAX_DELAY);
                break;
            }
            continue;
        }

        if (ch == '\r' || ch == '\n') {
            usb_serial_jtag_write_bytes("\r\n", 2, portMAX_DELAY);
            break;
        }

        if (ch == '\b' || ch == 0x7f) {
            if (!overflow && idx > 0) {
                idx--;
                usb_serial_jtag_write_bytes("\b \b", 3, portMAX_DELAY);
            }
            continue;
        }

        if (idx + 1 < len) {
            buf[idx++] = ch;
            usb_serial_jtag_write_bytes(&ch, 1, portMAX_DELAY);
        } else {
            overflow = true;
        }
    }

    buf[idx] = '\0';
    return (int)idx;
}

void input_write(const char *text)
{
    if (!text) {
        return;
    }
    input_write_bytes(text, strlen(text));
}

void input_write_bytes(const void *data, size_t len)
{
    if (!data || len == 0) {
        return;
    }
    usb_serial_jtag_write_bytes(data, len, portMAX_DELAY);
}

esp_err_t input_ble_hid_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

bool input_ble_hid_ready(void)
{
    return false;
}

bool input_ble_hid_boot_key_to_ascii(uint8_t modifier, uint8_t keycode, char *ascii)
{
    (void)modifier;
    (void)keycode;
    if (ascii) {
        *ascii = '\0';
    }
    return false;
}
