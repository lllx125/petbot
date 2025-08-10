#ifndef MOTOR_H
#define MOTOR_H

#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#ifdef __cplusplus
extern "C" {
#endif

// Motor configuration structure
typedef struct {
    gpio_num_t pin_pwm;     // PWM pin for speed control
    gpio_num_t pin_dir1;    // Direction pin 1
    gpio_num_t pin_dir2;    // Direction pin 2
    ledc_channel_t pwm_channel;
    ledc_timer_t pwm_timer;
} motor_config_t;

// Motor direction enum
typedef enum {
    MOTOR_STOP = 0,
    MOTOR_FORWARD = 1,
    MOTOR_BACKWARD = 2
} motor_direction_t;

// Motor structure
typedef struct {
    motor_config_t config;
    bool is_initialized;
} motor_t;

/**
 * @brief Initialize motor control system
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t motor_init(void);

/**
 * @brief Configure a motor
 * @param motor_id Motor identifier (0-3 for 4 motors)
 * @param config Motor configuration
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t motor_configure(uint8_t motor_id, const motor_config_t *config);

/**
 * @brief Set motor speed and direction
 * @param motor_id Motor identifier (0-3)
 * @param direction Motor direction (FORWARD/BACKWARD/STOP)
 * @param speed Speed value (0-100%)
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t motor_set_speed(uint8_t motor_id, motor_direction_t direction, uint8_t speed);

/**
 * @brief Stop a specific motor
 * @param motor_id Motor identifier (0-3)
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t motor_stop(uint8_t motor_id);

/**
 * @brief Stop all motors
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t motor_stop_all(void);

/**
 * @brief Get motor status
 * @param motor_id Motor identifier (0-3)
 * @param direction Pointer to store current direction
 * @param speed Pointer to store current speed
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t motor_get_status(uint8_t motor_id, motor_direction_t *direction, uint8_t *speed);

/**
 * @brief Deinitialize motor control system
 * @return ESP_OK on success, ESP_FAIL on error
 */
esp_err_t motor_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // MOTOR_H