#ifndef BATTERY_H
#define BATTERY_H

#include "esp_err.h"
#include "driver/adc.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t battery_init(adc1_channel_t adc_channel);

float battery_read_voltage(void);

float battery_read_percentage(void);

#ifdef __cplusplus
}
#endif
#endif // BATTERY_H



