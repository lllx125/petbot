#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize battery monitoring on a given GPIO
 * 
 * @param gpio_num GPIO number where battery voltage is connected
 * @return esp_err_t ESP_OK on success
 */
esp_err_t battery_init(gpio_num_t gpio_num);

/**
 * @brief Get the battery voltage in volts
 * 
 * @return float Battery voltage
 */
float battery_get_voltage(void);

/**
 * @brief Get the battery percentage based on voltage
 * 
 * @return int Percentage from 0 to 100
 */
int battery_get_percentage(void);

#ifdef __cplusplus
}
#endif
