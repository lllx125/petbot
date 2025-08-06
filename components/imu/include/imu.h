#ifndef IMU_H
#define IMU_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        float x;
        float y;
        float z;
    } imu_vector_t;

    typedef struct
    {
        imu_vector_t accelerometer;
        imu_vector_t gyroscope;
    } imu_data_t;

    esp_err_t imu_init(void);
    esp_err_t imu_read_data(imu_data_t *data);
    esp_err_t imu_calibrate(void);
    bool imu_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // IMU_H