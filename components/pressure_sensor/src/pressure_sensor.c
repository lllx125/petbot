#include "pressure_sensor.h"
#include "driver/adc.h"
#include "esp_log.h"

#define NO_OF_SAMPLES   64   // Multisampling

static const char *TAG = "PRESSURE_SENSOR";
static bool initialized = false;

esp_err_t pressure_sensor_init(void)
{
    if (initialized) {
        return ESP_OK;
    }

    // Configure ADC1 channel 0 (GPIO1)
    adc1_config_width(ADC_WIDTH_BIT_12);
    esp_err_t err = adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel");
        return err;
    }

    initialized = true;
    ESP_LOGI(TAG, "Pressure sensor initialized on GPIO1 (ADC1_CH0)");
    return ESP_OK;
}

esp_err_t pressure_sensor_read_raw(uint32_t *value)
{
    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!initialized) {
        ESP_LOGE(TAG, "Pressure sensor not initialized");
        return ESP_FAIL;
    }

    uint32_t adc_reading = 0;
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        adc_reading += adc1_get_raw(ADC1_CHANNEL_0);
    }
    adc_reading /= NO_OF_SAMPLES;

    *value = adc_reading;
    return ESP_OK;
}
