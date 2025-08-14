#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize I2S peripheral for INMP441 microphone.
 * 
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t microphone_init(void);

/**
 * @brief Read PCM samples from microphone.
 * 
 * @param buffer   Pointer to int16_t array for audio samples.
 * @param samples  Number of samples to read.
 * @return Number of samples actually read.
 */
size_t microphone_read(int16_t *buffer, size_t samples);

#ifdef __cplusplus
}
#endif
