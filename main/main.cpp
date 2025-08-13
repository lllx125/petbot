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

#include <esp_system.h>
#include <nvs_flash.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "camera.h"
#include "connect_wifi.h"

static const char *TAG = "MAIN";

extern "C" void app_main()
{
    esp_err_t err;

    ESP_LOGI(TAG, "Starting application");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_LOGI(TAG, "Connecting to WiFi");
    connect_wifi();

    if (wifi_connect_status)
    {
        ESP_LOGI(TAG, "WiFi connected, initializing camera");
        err = camera_init();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
            return;
        }
        
        ESP_LOGI(TAG, "Starting camera web server");
        camera_start_server();
        ESP_LOGI(TAG, "ESP32 CAM Web Server is up and running");
        
        // Take a picture every 5 seconds for testing
        while (1) {
            camera_take_picture();
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi, check your network credentials");
    }
}