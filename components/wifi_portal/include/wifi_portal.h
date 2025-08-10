#pragma once
#include <stdbool.h>
#include "esp_netif_ip_addr.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Start the Wi-Fi manager:
    //  • If credentials exist in NVS, try STA connect.
    //  • On repeated failure OR if no creds: start SoftAP + web portal.
    //  • Returns true when connected to STA (user Wi-Fi). Blocks until
    //    either connected or portal is up (non-blocking thereafter).
    bool wifi_portal_start(void);

    // Force clearing saved SSID/PASS from NVS (call before wifi_portal_start()).
    void wifi_portal_erase_credentials(void);

    // Get current STA IP (valid only after connected == true).
    bool wifi_portal_get_sta_ip(esp_ip4_addr_t *out_ip);

    // Optional: set how many STA retries before falling back to portal (default 5).
    void wifi_portal_set_sta_retry_limit(int retries);

    // Optional: set the password for the setup SoftAP (NULL or "" => open AP).
    void wifi_portal_set_ap_password(const char *ap_pass);

    // Optional: set the SoftAP SSID prefix (default "Petbot").
    void wifi_portal_set_ap_ssid_prefix(const char *prefix);

#ifdef __cplusplus
}
#endif
