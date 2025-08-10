#pragma once
#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Minimal UDP DNS server that replies to any A query with given IPv4 address.
    // Only needed while running as SoftAP to support a captive portal style hostname
    // like "petbot.com". On STA we instead advertise mDNS name "petbot.local".

    esp_err_t dns_server_start(uint32_t ip_be); // ip in network byte order
    void dns_server_stop(void);

#ifdef __cplusplus
}
#endif