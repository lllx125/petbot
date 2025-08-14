#include "microphone.h"
#include "driver/i2s.h"
#include "esp_log.h"

static const char *TAG = "MICROPHONE";

#define MIC_I2S_PORT        I2S_NUM_0
#define MIC_SAMPLE_RATE     16000
#define MIC_SAMPLE_BITS     32        // INMP441 outputs 32-bit frames
#define MIC_DMA_BUF_COUNT   4
#define MIC_DMA_BUF_LEN     256

// Change these to match your wiring
#define MIC_I2S_WS      47   // LRCL (word select / WS)
#define MIC_I2S_SD      48   // DOUT from mic
#define MIC_I2S_SCK     21   // BCLK

/**
 * Initialize the I2S driver for the INMP441 microphone.
 * 
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t microphone_init(void)
{
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = MIC_SAMPLE_RATE,
        .bits_per_sample = MIC_SAMPLE_BITS,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = MIC_DMA_BUF_COUNT,
        .dma_buf_len = MIC_DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = MIC_I2S_SCK,
        .ws_io_num = MIC_I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE, // mic is input only
        .data_in_num = MIC_I2S_SD
    };

    ESP_LOGI(TAG, "Installing I2S driver");
    esp_err_t ret = i2s_driver_install(MIC_I2S_PORT, &i2s_config, 0, NULL);
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "Setting I2S pins");
    ret = i2s_set_pin(MIC_I2S_PORT, &pin_config);
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "Microphone ready: %d Hz, %d-bit frames", MIC_SAMPLE_RATE, MIC_SAMPLE_BITS);
    return ESP_OK;
}

/**
 * Read sound amplitude from the microphone
 * 
 * @param buffer   Pointer to int16_t array for audio samples.
 * @param samples  Number of samples to read.
 * @return Number of samples actually read.
 */
size_t microphone_read(int16_t *buffer, size_t samples)
{
    // Temporary buffer for raw 32-bit I2S data
    int32_t raw_buffer[256];
    if (samples > 256) samples = 256;

    size_t bytes_read = 0;
    esp_err_t ret = i2s_read(MIC_I2S_PORT, raw_buffer, samples * sizeof(int32_t), &bytes_read, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S read error: %s", esp_err_to_name(ret));
        return 0;
    }

    size_t samples_read = bytes_read / sizeof(int32_t);

    // Convert from 32-bit I2S to 16-bit PCM
    for (size_t i = 0; i < samples_read; i++) {
        buffer[i] = (int16_t)(raw_buffer[i] >> 14); // Shift down, keep significant bits
    }

    return samples_read;
}
