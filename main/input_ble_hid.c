#include "input.h"

#include "esp_log.h"

static const char *TAG = "input_ble_hid";

#define HID_MOD_LEFT_SHIFT  0x02
#define HID_MOD_RIGHT_SHIFT 0x20

esp_err_t input_ble_hid_init(void)
{
    ESP_LOGI(TAG, "BLE HID keyboard backend is present but not enabled");
    return ESP_ERR_NOT_SUPPORTED;
}

bool input_ble_hid_ready(void)
{
    return false;
}

bool input_ble_hid_boot_key_to_ascii(uint8_t modifier, uint8_t keycode, char *ascii)
{
    if (!ascii) {
        return false;
    }

    bool shift = (modifier & (HID_MOD_LEFT_SHIFT | HID_MOD_RIGHT_SHIFT)) != 0;

    if (keycode >= 0x04 && keycode <= 0x1d) {
        char base = (char)('a' + (keycode - 0x04));
        *ascii = shift ? (char)(base - 'a' + 'A') : base;
        return true;
    }

    if (keycode >= 0x1e && keycode <= 0x26) {
        static const char normal[] = "123456789";
        static const char shifted[] = "!@#$%^&*(";
        *ascii = shift ? shifted[keycode - 0x1e] : normal[keycode - 0x1e];
        return true;
    }

    if (keycode == 0x27) {
        *ascii = shift ? ')' : '0';
        return true;
    }

    switch (keycode) {
    case 0x2c: *ascii = ' '; return true;
    case 0x2d: *ascii = shift ? '_' : '-'; return true;
    case 0x2e: *ascii = shift ? '+' : '='; return true;
    case 0x2f: *ascii = shift ? '{' : '['; return true;
    case 0x30: *ascii = shift ? '}' : ']'; return true;
    case 0x31: *ascii = shift ? '|' : '\\'; return true;
    case 0x33: *ascii = shift ? ':' : ';'; return true;
    case 0x34: *ascii = shift ? '"' : '\''; return true;
    case 0x35: *ascii = shift ? '~' : '`'; return true;
    case 0x36: *ascii = shift ? '<' : ','; return true;
    case 0x37: *ascii = shift ? '>' : '.'; return true;
    case 0x38: *ascii = shift ? '?' : '/'; return true;
    default:
        *ascii = '\0';
        return false;
    }
}
//Keep Going.
