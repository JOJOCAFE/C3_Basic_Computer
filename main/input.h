#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    INPUT_KEY_NONE = 0,
    INPUT_KEY_CHAR,
    INPUT_KEY_ENTER,
    INPUT_KEY_BACKSPACE,
    INPUT_KEY_ESCAPE,
    INPUT_KEY_LEFT,
    INPUT_KEY_RIGHT,
    INPUT_KEY_UP,
    INPUT_KEY_DOWN,
} input_key_t;

typedef struct {
    input_key_t key;
    uint8_t modifier;
    bool pressed;
    char ascii;
} input_event_t;

esp_err_t input_init(void);
int input_read_line(char *buf, size_t len);
void input_write(const char *text);
void input_write_bytes(const void *data, size_t len);

esp_err_t input_ble_hid_init(void);
bool input_ble_hid_ready(void);
bool input_ble_hid_boot_key_to_ascii(uint8_t modifier, uint8_t keycode, char *ascii);
