#pragma once
#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C"
{
#endif

    esp_err_t petbot_web_start(httpd_handle_t *server_out);
    void petbot_web_stop(httpd_handle_t server);

    // Expose log push API for other modules
    void petbot_log_push_line(const char *line);

#ifdef __cplusplus
}
#endif