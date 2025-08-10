#include <stdio.h>
#include "esp_log.h"
#include "wifi_portal.h"

extern "C" void app_main(void)
{
    // Optional: allow factory reset by holding a button at boot (pseudo-code)
    // if (gpio_get_level(RESET_PIN) == 0) wifi_portal_erase_credentials();

    // Optional: tweak behavior
    wifi_portal_set_sta_retry_limit(5);
    wifi_portal_set_ap_ssid_prefix("Petbot");
    // wifi_portal_set_ap_password("petbot-setup"); // set a password for the portal AP

    bool ok = wifi_portal_start(); // blocks until connected OR portal started and then connected
    if (ok)
    {
        esp_ip4_addr_t ip;
        if (wifi_portal_get_sta_ip(&ip))
        {
            ESP_LOGI("APP", "Connected! IP: " IPSTR, IP2STR(&ip));
        }
    }
    else
    {
        ESP_LOGE("APP", "Unexpected: portal ended without connection");
    }

    // Start the rest of your app here (HTTP API, MQTT, eyes, etc.)
}
