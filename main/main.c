#include "bin.h"
#include "shell.h"
#include "storage.h"

#include "esp_log.h"

static const char *TAG = "c3main";

void app_main(void)
{
    ESP_LOGI(TAG, "Booting C3 BASIC COMPUTER");

    if (storage_init() != ESP_OK) {
        ESP_LOGE(TAG, "storage init failed");
        return;
    }

    shell_register_external_exec(bin_exec_line);
    shell_run();
}
