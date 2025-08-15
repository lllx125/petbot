#include "battery.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_err.h"

#define DEFAULT_VREF 1100    // mV
#define NO_OF_SAMPLES 64
#define VOLTAGE_DIVIDER 2.0f  // Adjust if you use a different resistor divider

static esp_adc_cal_characteristics_t adc_chars;
static adc1_channel_t adc_channel;

static int gpio_to_adc_channel(gpio_num_t gpio) {
    // Map GPIO to ADC1 channel for ESP32-S3
    switch(gpio) {
        case GPIO_NUM_1: return ADC1_CHANNEL_0;
        case GPIO_NUM_2: return ADC1_CHANNEL_1;
        case GPIO_NUM_3: return ADC1_CHANNEL_2;
        case GPIO_NUM_4: return ADC1_CHANNEL_3;
        case GPIO_NUM_5: return ADC1_CHANNEL_4;
        case GPIO_NUM_6: return ADC1_CHANNEL_5;
        case GPIO_NUM_34: return ADC1_CHANNEL_6;  
        case GPIO_NUM_35: return ADC1_CHANNEL_7;
        default: return -1; // Invalid / unsupported
    }
}

esp_err_t battery_init(gpio_num_t gpio_num) {
    int channel = gpio_to_adc_channel(gpio_num);
    if (channel < 0) return ESP_ERR_INVALID_ARG;

    adc_channel = (adc1_channel_t)channel;

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(adc_channel, ADC_ATTEN_DB_11);

    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11,
                             ADC_WIDTH_BIT_12, DEFAULT_VREF, &adc_chars);

    return ESP_OK;
}

float battery_get_voltage(void) {
    uint32_t adc_reading = 0;
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        adc_reading += adc1_get_raw(adc_channel);
    }
    adc_reading /= NO_OF_SAMPLES;

    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(adc_reading, &adc_chars);

    // Apply voltage divider factor
    return (voltage_mv / 1000.0f) * VOLTAGE_DIVIDER;
}

int battery_get_percentage(void) {
    float voltage = battery_get_voltage();

    if (voltage >= 4.20f) return 100;
    if (voltage <= 3.00f) return 0;

    return (int)((voltage - 3.0f) / (4.2f - 3.0f) * 100);
}
