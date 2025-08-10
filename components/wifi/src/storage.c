#include "storage.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>
#include <stdlib.h>

#define NS "wifi_store"

typedef struct
{
    uint32_t count;
    wifi_cred_t list[8]; // keep small; increase if you like
} blob_t;

static esp_err_t load_blob(blob_t *b)
{
    memset(b, 0, sizeof(*b));
    nvs_handle_t h;
    esp_err_t e = nvs_open(NS, NVS_READONLY, &h);
    if (e != ESP_OK)
        return e;
    size_t sz = sizeof(*b);
    e = nvs_get_blob(h, "creds", b, &sz);
    nvs_close(h);
    return e;
}

static esp_err_t save_blob(const blob_t *b)
{
    nvs_handle_t h;
    esp_err_t e = nvs_open(NS, NVS_READWRITE, &h);
    if (e != ESP_OK)
        return e;
    e = nvs_set_blob(h, "creds", b, sizeof(*b));
    if (e == ESP_OK)
        e = nvs_commit(h);
    nvs_close(h);
    return e;
}

esp_err_t storage_load_creds(wifi_cred_t **out, size_t *count)
{
    blob_t b;
    esp_err_t e = load_blob(&b);
    if (e != ESP_OK || b.count == 0)
    {
        *out = NULL;
        *count = 0;
        return ESP_OK;
    }
    *count = b.count;
    *out = calloc(b.count, sizeof(wifi_cred_t));
    memcpy(*out, b.list, b.count * sizeof(wifi_cred_t));
    return ESP_OK;
}

esp_err_t storage_save_or_update(const wifi_cred_t *c)
{
    blob_t b;
    load_blob(&b); // OK if empty
    // Replace if SSID exists
    for (uint32_t i = 0; i < b.count; ++i)
    {
        if (strncmp(b.list[i].ssid, c->ssid, sizeof(c->ssid)) == 0)
        {
            b.list[i] = *c;
            return save_blob(&b);
        }
    }
    if (b.count >= (sizeof(b.list) / sizeof(b.list[0])))
        return ESP_ERR_NO_MEM;
    b.list[b.count++] = *c;
    return save_blob(&b);
}

esp_err_t storage_delete_ssid(const char *ssid)
{
    blob_t b;
    esp_err_t e = load_blob(&b);
    if (e != ESP_OK)
        return e;
    uint32_t j = 0;
    bool found = false;
    for (uint32_t i = 0; i < b.count; ++i)
    {
        if (strncmp(b.list[i].ssid, ssid, sizeof(b.list[i].ssid)) == 0)
        {
            found = true;
            continue;
        }
        b.list[j++] = b.list[i];
    }
    if (!found)
        return ESP_ERR_NOT_FOUND;
    b.count = j;
    return save_blob(&b);
}

esp_err_t storage_clear_all(void)
{
    blob_t b = {0};
    return save_blob(&b);
}