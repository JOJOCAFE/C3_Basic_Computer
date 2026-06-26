#pragma once

#include <stddef.h>
#include <stdint.h>

#define C3_COM_MAGIC_0 'C'
#define C3_COM_MAGIC_1 '3'
#define C3_COM_MAGIC_2 'C'
#define C3_COM_MAGIC_3 'M'
#define C3_COM_ABI_VERSION 1u
#define C3_COM_TARGET_ESP32C3_RV32 1u
#define C3_COM_MAX_CODE_SIZE (64u * 1024u)

typedef struct __attribute__((packed)) {
    uint8_t magic[4];
    uint16_t header_size;
    uint16_t abi_version;
    uint32_t target;
    uint32_t flags;
    uint32_t code_offset;
    uint32_t code_size;
    uint32_t entry_offset;
    uint32_t code_crc32;
    uint32_t reserved[4];
} c3_com_header_t;

typedef struct {
    uint32_t abi_version;
    int argc;
    const char *const *argv;
    int (*stdin_read_line)(char *buf, size_t len);
    void (*stdout_write)(const void *data, size_t len);
    void (*stderr_write)(const void *data, size_t len);
} c3_com_api_t;

typedef int (*c3_com_entry_fn)(const c3_com_api_t *api, int argc, const char *const argv[]);

//Keep Going.
