#include "imu.h"
#include "esp_err.h"
#include <string.h>

//testtest

static bool initialized = false;

esp_err_t imu_init(void)
{
    esp_err_t ret = ESP_OK;

    // TODO: Add actual IMU initialization code here

    initialized = true;

    return ret;
}

esp_err_t imu_read_data(imu_data_t *data)
{
    if (!initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // TODO: Add actual IMU data reading code here
    memset(data, 0, sizeof(imu_data_t));

    // Dummy data for testing
    data->accelerometer.x = 0.0f;
    data->accelerometer.y = 0.0f;
    data->accelerometer.z = 9.8f;

    data->gyroscope.x = 0.0f;
    data->gyroscope.y = 0.0f;
    data->gyroscope.z = 0.0f;

    return ESP_OK;
}

esp_err_t imu_calibrate(void)
{
    if (!initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    // TODO: Add actual IMU calibration code here

    return ESP_OK;
}

bool imu_is_initialized(void)
{
    return initialized;
}