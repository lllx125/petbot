#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char ssid[33];
    char pass[65]; // store pass; return masked to UI
} wifi_cred_t;

// A very simple list stored in NVS: count + array
esp_err_t storage_load_creds(wifi_cred_t **out, size_t *count);
esp_err_t storage_save_or_update(const wifi_cred_t *c); // add or replace by SSID
esp_err_t storage_delete_ssid(const char *ssid);
esp_err_t storage_clear_all(void);

#ifdef __cplusplus
}
#endif