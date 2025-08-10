#pragma once
#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Basic configuration you can tweak from main
    typedef struct
    {
        const char *ap_ssid; // SoftAP SSID when we fail to join STA
        const char *ap_pass; // SoftAP password (>=8 chars or empty for open)
        uint8_t ap_channel;  // 1..13
    } petbot_net_cfg_t;

    // Start the Wi‑Fi + HTTP stack.
    esp_err_t petbot_wifi_start(const petbot_net_cfg_t *cfg);

    // A tiny "ESPLOG to web" helper you can call from anywhere.
    void petbot_web_logf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif