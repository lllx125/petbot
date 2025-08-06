#include "Arduino.h"
#include "esp_log.h"
#include "imu.h"

extern "C" void app_main()
{
    initArduino();
    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH);
    // Do your own thing
    imu_init();
    while (1)
    {
        imu_data_t data;
        if (imu_read_data(&data) == ESP_OK)
        {
            ESP_LOGI("MAIN", "Accelerometer: x=%.2f, y=%.2f, z=%.2f",
                     data.accelerometer.x, data.accelerometer.y, data.accelerometer.z);
            ESP_LOGI("MAIN", "Gyroscope: x=%.2f, y=%.2f, z=%.2f",
                     data.gyroscope.x, data.gyroscope.y, data.gyroscope.z);
        }
        else
        {
            ESP_LOGE("MAIN", "Failed to read IMU data");
        }
        delay(10); // Delay for 1 second
    }
}