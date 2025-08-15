/*
for imu
*/
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

/*
for camera
*/
// #include "esp_log.h"
// #include "nvs_flash.h"
// #include "camera.h"
// #include "esp_event.h"
// #include "esp_netif.h"
// #include "esp_wifi.h"

// static const char *TAG = "main";

// #define WIFI_SSID      "ASUS" // Replace with your Wi-Fi SSID
// #define WIFI_PASS      "5105566208" // Replace with your Wi-Fi password


// static void wifi_event_handler(void *arg, esp_event_base_t event_base,
//                                int32_t event_id, void *event_data) {
//     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
//         esp_wifi_connect();
//     } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
//         ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
//         ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
//         start_camera_server();  // Start camera server when Wi-Fi connected
//     }
// }

// void wifi_init_sta() {
//     ESP_LOGI(TAG, "Initializing Wi-Fi...");
//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());
//     esp_netif_create_default_wifi_sta();

//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
//                                                         ESP_EVENT_ANY_ID,
//                                                         &wifi_event_handler,
//                                                         NULL,
//                                                         NULL));
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
//                                                         IP_EVENT_STA_GOT_IP,
//                                                         &wifi_event_handler,
//                                                         NULL,
//                                                         NULL));

//     wifi_config_t wifi_config = {};
//     strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
//     strncpy((char *)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password));
//     wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());

//     ESP_LOGI(TAG, "WiFi init finished");
// }

// extern "C" void app_main(void) {
//     ESP_LOGI(TAG, "Starting PetBot application...");
//     ESP_ERROR_CHECK(nvs_flash_init());
//     init_camera();  // Initialize camera hardware
//     wifi_init_sta(); // Connect to Wi-Fi
// }

/*
for microphone
*/
// #include <stdio.h>
// #include "microphone.h"
// #include "esp_log.h"

// extern "C" void app_main(void)
// {
//     esp_err_t ret = microphone_init();
//     if (ret != ESP_OK) {
//         ESP_LOGE("MAIN", "Microphone init failed: %s", esp_err_to_name(ret));
//         return;
//     }

//     int16_t buffer[256];

//     while (1) {
//         size_t samples = microphone_read(buffer, 256);
//         for (size_t i = 0; i < samples; i++) {
//             printf("%d\n", buffer[i]); // One value per line
//         }
//     }
// }

/*
for battery
*/

#include "battery.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "battery.h"
#include <stdio.h>


extern "C" void app_main(void) {
    // Use GPIO34 for battery
    battery_init(GPIO_NUM_2);

    while(1) {
        float voltage = battery_get_voltage();
        int percent = battery_get_percentage();
        printf("Battery: %.2f V (%d%%)\n", voltage, percent);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

