#ifndef PRESSURE_SENSOR_H
#define PRESSURE_SENSOR_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the pressure sensor (ADC on GPIO1).
 * 
 * @return
 *  - ESP_OK on success
 *  - ESP_FAIL if initialization fails
 */
esp_err_t pressure_sensor_init(void);

/**
 * @brief Read raw ADC value from the pressure sensor.
 * 
 * @param[out] value Pointer to store raw ADC reading (0–4095).
 * @return
 *  - ESP_OK on success
 *  - ESP_ERR_INVALID_ARG if value is NULL
 */
esp_err_t pressure_sensor_read_raw(uint32_t *value);

#ifdef __cplusplus
}
#endif

#endif // PRESSURE_SENSOR_H
