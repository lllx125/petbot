#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int sample_rate;
    int bits_per_sample; // e.g., 16
    int channel_format;  // 1 = mono, 2 = stereo
} speaker_config_t;

/**
 * @brief Initialize speaker (MAX98357A) with I2S
 */
esp_err_t speaker_init(const speaker_config_t *config);

/**
 * @brief Play a PCM buffer
 * @param buffer PCM samples (16-bit signed)
 * @param num_samples Number of samples
 */
esp_err_t speaker_play(const int16_t *buffer, size_t num_samples);

#ifdef __cplusplus
}
#endif
