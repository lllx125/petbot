#include "motor.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include <string.h>

static const char *TAG = "MOTOR";

#define MAX_MOTORS 4
#define PWM_FREQUENCY 1000
#define PWM_RESOLUTION LEDC_TIMER_8_BIT
#define PWM_MAX_DUTY ((1 << PWM_RESOLUTION) - 1)

static motor_t motors[MAX_MOTORS];
static bool motor_system_initialized = false;

esp_err_t motor_init(void)
{
    if (motor_system_initialized) {
        ESP_LOGW(TAG, "Motor system already initialized");
        return ESP_OK;
    }

    // Initialize motor array
    memset(motors, 0, sizeof(motors));

    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = PWM_RESOLUTION,
        .freq_hz = PWM_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }

    motor_system_initialized = true;
    ESP_LOGI(TAG, "Motor control system initialized");
    return ESP_OK;
}

esp_err_t motor_configure(uint8_t motor_id, const motor_config_t *config)
{
    if (!motor_system_initialized) {
        ESP_LOGE(TAG, "Motor system not initialized");
        return ESP_FAIL;
    }

    if (motor_id >= MAX_MOTORS) {
        ESP_LOGE(TAG, "Invalid motor ID: %d", motor_id);
        return ESP_FAIL;
    }

    if (config == NULL) {
        ESP_LOGE(TAG, "Motor config is NULL");
        return ESP_FAIL;
    }

    // Configure GPIO pins for direction control
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << config->pin_dir1) | (1ULL << config->pin_dir2),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO pins: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure LEDC channel for PWM
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = config->pwm_channel,
        .timer_sel = config->pwm_timer,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = config->pin_pwm,
        .duty = 0,
        .hpoint = 0
    };
    ret = ledc_channel_config(&ledc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // Store configuration
    motors[motor_id].config = *config;
    motors[motor_id].is_initialized = true;

    // Initialize motor in stopped state
    gpio_set_level(config->pin_dir1, 0);
    gpio_set_level(config->pin_dir2, 0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, config->pwm_channel, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, config->pwm_channel);

    ESP_LOGI(TAG, "Motor %d configured successfully", motor_id);
    return ESP_OK;
}

esp_err_t motor_set_speed(uint8_t motor_id, motor_direction_t direction, uint8_t speed)
{
    if (!motor_system_initialized) {
        ESP_LOGE(TAG, "Motor system not initialized");
        return ESP_FAIL;
    }

    if (motor_id >= MAX_MOTORS) {
        ESP_LOGE(TAG, "Invalid motor ID: %d", motor_id);
        return ESP_FAIL;
    }

    if (!motors[motor_id].is_initialized) {
        ESP_LOGE(TAG, "Motor %d not configured", motor_id);
        return ESP_FAIL;
    }

    if (speed > 100) {
        ESP_LOGE(TAG, "Invalid speed: %d (must be 0-100)", speed);
        return ESP_FAIL;
    }

    motor_config_t *config = &motors[motor_id].config;
    
    // Set direction pins
    switch (direction) {
        case MOTOR_STOP:
            gpio_set_level(config->pin_dir1, 0);
            gpio_set_level(config->pin_dir2, 0);
            speed = 0; // Ensure speed is 0 when stopping
            break;
        
        case MOTOR_FORWARD:
            gpio_set_level(config->pin_dir1, 1);
            gpio_set_level(config->pin_dir2, 0);
            break;
        
        case MOTOR_BACKWARD:
            gpio_set_level(config->pin_dir1, 0);
            gpio_set_level(config->pin_dir2, 1);
            break;
        
        default:
            ESP_LOGE(TAG, "Invalid direction: %d", direction);
            return ESP_FAIL;
    }

    // Convert speed percentage to PWM duty cycle
    uint32_t duty = (speed * PWM_MAX_DUTY) / 100;
    
    // Set PWM duty cycle
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, config->pwm_channel, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set PWM duty: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, config->pwm_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update PWM duty: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Motor %d set to direction %d, speed %d%%", motor_id, direction, speed);
    return ESP_OK;
}

esp_err_t motor_stop(uint8_t motor_id)
{
    return motor_set_speed(motor_id, MOTOR_STOP, 0);
}

esp_err_t motor_stop_all(void)
{
    esp_err_t ret = ESP_OK;
    
    for (uint8_t i = 0; i < MAX_MOTORS; i++) {
        if (motors[i].is_initialized) {
            esp_err_t motor_ret = motor_stop(i);
            if (motor_ret != ESP_OK) {
                ret = motor_ret;
            }
        }
    }
    
    ESP_LOGI(TAG, "All motors stopped");
    return ret;
}

esp_err_t motor_get_status(uint8_t motor_id, motor_direction_t *direction, uint8_t *speed)
{
    if (!motor_system_initialized) {
        ESP_LOGE(TAG, "Motor system not initialized");
        return ESP_FAIL;
    }

    if (motor_id >= MAX_MOTORS) {
        ESP_LOGE(TAG, "Invalid motor ID: %d", motor_id);
        return ESP_FAIL;
    }

    if (!motors[motor_id].is_initialized) {
        ESP_LOGE(TAG, "Motor %d not configured", motor_id);
        return ESP_FAIL;
    }

    if (direction == NULL || speed == NULL) {
        ESP_LOGE(TAG, "Output parameters are NULL");
        return ESP_FAIL;
    }

    motor_config_t *config = &motors[motor_id].config;
    
    // Read direction pins
    int dir1 = gpio_get_level(config->pin_dir1);
    int dir2 = gpio_get_level(config->pin_dir2);
    
    if (dir1 == 0 && dir2 == 0) {
        *direction = MOTOR_STOP;
    } else if (dir1 == 1 && dir2 == 0) {
        *direction = MOTOR_FORWARD;
    } else if (dir1 == 0 && dir2 == 1) {
        *direction = MOTOR_BACKWARD;
    } else {
        ESP_LOGW(TAG, "Invalid direction pin state for motor %d", motor_id);
        *direction = MOTOR_STOP;
    }

    // Get current PWM duty cycle and convert to speed percentage
    uint32_t duty = ledc_get_duty(LEDC_LOW_SPEED_MODE, config->pwm_channel);
    *speed = (duty * 100) / PWM_MAX_DUTY;

    return ESP_OK;
}

esp_err_t motor_deinit(void)
{
    if (!motor_system_initialized) {
        ESP_LOGW(TAG, "Motor system not initialized");
        return ESP_OK;
    }

    // Stop all motors
    motor_stop_all();

    // Reset motor configurations
    memset(motors, 0, sizeof(motors));
    motor_system_initialized = false;

    ESP_LOGI(TAG, "Motor control system deinitialized");
    return ESP_OK;
}