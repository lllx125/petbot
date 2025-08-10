#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi.h"

static const char *TAG = "main";

void initialize(void);

void app_main(void)
{

    initialize(); // Initialize NVS and Wi-Fi

    // 3) Example: push some logs to the web log page
    for (int i = 1;; ++i)
    {
        petbot_web_logf("Heartbeat %d: free heap=%lu", i, (unsigned long)esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void initialize(void)
{
    // 1) Initialize NVS (for Wi‑Fi, credentials, OTA markers etc.)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // 2) Bring up Wi‑Fi + web server (STA first, fall back to AP)
    petbot_net_cfg_t cfg = {
        .ap_ssid = "PetBot-Setup",
        .ap_pass = "petbot123", // keep simple for demo; change in production
        .ap_channel = 6,
    };
    ESP_ERROR_CHECK(petbot_wifi_start(&cfg));
}