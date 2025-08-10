#include "Arduino.h"
#include "esp_log.h"
#include "imu.h"

extern "C" void app_main()
{
    initArduino();
    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH);

    if (imu_init() == ESP_OK)
        ESP_LOGI("MAIN", "IMU initialized");
    else
        ESP_LOGE("MAIN", "IMU init failed");

    while (1)
    {
        imu_data_t data;
        if (imu_read_data(&data) == ESP_OK)
        {
            ESP_LOGI("MAIN", "Accel: x=%.2f y=%.2f z=%.2f m/s²",
                     data.accelerometer.x, data.accelerometer.y, data.accelerometer.z);
            ESP_LOGI("MAIN", "Gyro: x=%.2f y=%.2f z=%.2f dps",
                     data.gyroscope.x, data.gyroscope.y, data.gyroscope.z);
        }
        delay(500);
    }
}

