#ifndef CAMERA_H
#define CAMERA_H

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t camera_init(void);
httpd_handle_t camera_start_server(void);
esp_err_t camera_take_picture(void);

#ifdef __cplusplus
}
#endif

#endif // CAMERA_H