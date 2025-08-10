#pragma once
#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Register the /api/ota/upload handler with an existing server
    esp_err_t ota_register_handlers(httpd_handle_t server);

#ifdef __cplusplus
}
#endif