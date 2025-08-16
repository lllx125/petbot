#include "battery.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"

#define TAG "BATTERY"

// LiPo battery limits (in volts)
#define BATTERY_MIN_VOLTAGE 3.0f //Voltage treated as 0%
#define BATTERY_MAX_VOLTAGE 4.2f //Voltage treated as 100%

// Voltage divider factor (two 10k resistors -> measured voltage is half)
#define VOLTAGE_DIVIDER_RATIO 2.0f //x2 for voltage divider

// ADC config
#define ADC_WIDTH ADC_WIDTH_BIT_12 //ADC resolution: 12 bits (0..4095)
#define ADC_ATTEN ADC_ATTEN_DB_11 // 0-3.9V approx range

static adc1_channel_t battery_adc_channel;           // <-- same type
static esp_adc_cal_characteristics_t adc_chars; //calibration data for raw->mV conversion

/**
 * @brief Initialize the battery ADC
 * 
 * This function configures the ADC channel for battery voltage measurement.
 * It sets the ADC width and attenuation, and characterizes the ADC for voltage readings.
 * 
 * @param adc_channel The ADC channel connected to the voltage divider
 * @return esp_err_t ESP_OK on success
 */
esp_err_t battery_init(adc1_channel_t adc_channel)   // <-- adc1_channel_t, not adc_channel_t
{
    battery_adc_channel = adc_channel;

    // Configure ADC width and channel
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(battery_adc_channel, ADC_ATTEN);

    // Characterize ADC
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, 1100, &adc_chars);

    ESP_LOGI(TAG, "Battery ADC initialized on channel %d", adc_channel);
    return ESP_OK;
}

/**
 * @brief Read the battery voltage in volts
 * 
 * This function reads the raw ADC value, converts it to millivolts,
 * and compensates for the voltage divider to return the actual battery voltage.
 * 
 * @return float actual voltage value
 */
float battery_read_voltage(void)
{
    uint32_t raw = adc1_get_raw(battery_adc_channel);
    printf("Raw ADC value: %ld\n", raw); // Debugging line
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);

    // Compensate for voltage divider
    float actual_voltage = (voltage_mv / 1000.0f) * VOLTAGE_DIVIDER_RATIO;

    return actual_voltage;
}

/**
 * @brief Read the battery percentage (0–100%)
 * 
 * This function calculates the battery percentage based on the voltage.
 * It clamps the voltage to the defined min and max limits before calculating the percentage.
 * 
 * @return float Battery percentage
 */
float battery_read_percentage(void)
{
    float voltage = battery_read_voltage();

    // Clamp voltage between min and max
    if (voltage < BATTERY_MIN_VOLTAGE) voltage = BATTERY_MIN_VOLTAGE;
    if (voltage > BATTERY_MAX_VOLTAGE) voltage = BATTERY_MAX_VOLTAGE;

    // Map to 0–100%
    float percent = ((voltage - BATTERY_MIN_VOLTAGE) /
                    (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE)) * 100.0f;

    return percent;
}
