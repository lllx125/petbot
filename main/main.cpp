// #include "Arduino.h"
// #include "esp_log.h"
// #include "imu.h"

// extern "C" void app_main()
// {
//     initArduino();
//     pinMode(4, OUTPUT);
//     digitalWrite(4, HIGH);

//     if (imu_init() == ESP_OK)
//         ESP_LOGI("MAIN", "IMU initialized");
//     else
//         ESP_LOGE("MAIN", "IMU init failed");

//     while (1)
//     {
//         imu_data_t data;
//         if (imu_read_data(&data) == ESP_OK)
//         {
//             ESP_LOGI("MAIN", "Accel: x=%.2f y=%.2f z=%.2f m/s²",
//                      data.accelerometer.x, data.accelerometer.y, data.accelerometer.z);
//             ESP_LOGI("MAIN", "Gyro: x=%.2f y=%.2f z=%.2f dps",
//                      data.gyroscope.x, data.gyroscope.y, data.gyroscope.z);
//         }
//         delay(500);
//     }
// }

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_camera.h"
#include "camera.h"
#include "esp_http_server.h"

static const char *TAG = "main";

#define WIFI_SSID      "ASUS"
#define WIFI_PASS      "5105566208"

static void start_camera_server();

// Wi-Fi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        start_camera_server();
        ESP_LOGI(TAG, "Camera Ready! Use 'http://%s' to connect",
                 ip4addr_ntoa(&event->ip_info.ip));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    }
}

static void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handler,
                                        NULL,
                                        &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler,
                                        NULL,
                                        &instance_got_ip);

    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASS);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(init_camera());
    wifi_init_sta();
}

// --- MJPEG Stream Handler ---
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART =
    "Content-Type: im

