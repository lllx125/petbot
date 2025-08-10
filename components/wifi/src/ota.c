#include "ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include <string.h>

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

static const char *TAG = "ota";

// Simple multipart‑less upload: send raw .bin as POST body to /api/ota/upload
static esp_err_t h_ota_upload(httpd_req_t *req)
{
    const esp_partition_t *update_part = esp_ota_get_next_update_partition(NULL);
    if (!update_part)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no partition");

    esp_ota_handle_t h;
    ESP_ERROR_CHECK(esp_ota_begin(update_part, req->content_len, &h));

    char buf[2048];
    int remaining = req->content_len;
    int received_total = 0;
    while (remaining > 0)
    {
        int r = httpd_req_recv(req, buf, MIN((int)sizeof(buf), remaining));
        if (r <= 0)
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv err");
        ESP_ERROR_CHECK(esp_ota_write(h, buf, r));
        remaining -= r;
        received_total += r;
    }

    ESP_ERROR_CHECK(esp_ota_end(h));
    ESP_ERROR_CHECK(esp_ota_set_boot_partition(update_part));

    httpd_resp_sendstr(req, "OK - Rebooting");
    ESP_LOGI(TAG, "OTA received %d bytes. Rebooting...", received_total);
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

esp_err_t ota_register_handlers(httpd_handle_t server)
{
    const httpd_uri_t uri = {.uri = "/api/ota/upload", .method = HTTP_POST, .handler = h_ota_upload};
    return httpd_register_uri_handler(server, &uri);
}