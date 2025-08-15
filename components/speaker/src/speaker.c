#include "speaker.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"  // for portMAX_DELAY
#include "freertos/task.h"      // optional, in case you use vTaskDelay


static const char *TAG = "SPEAKER";

static i2s_chan_handle_t tx_chan = NULL;

esp_err_t speaker_init(const speaker_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Slot config
    i2s_std_slot_config_t slot_cfg = {
        .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
        .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT,
        .slot_mode      = I2S_SLOT_MODE_MONO,
        .slot_mask      = I2S_STD_SLOT_LEFT,
        .ws_width       = I2S_DATA_BIT_WIDTH_16BIT,
        .ws_pol         = false,
        .bit_shift      = true,
        .left_align     = true,
        .big_endian     = false
    };

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config->sample_rate),
        .slot_cfg = slot_cfg,
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_21,  // adjust to your wiring
            .ws   = GPIO_NUM_48,
            .dout = GPIO_NUM_38,  // MAX98357A DIN
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {0}
        }
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    ESP_LOGI(TAG, "Speaker initialized @ %d Hz", config->sample_rate);
    return ESP_OK;
}

esp_err_t speaker_play(const int16_t *buffer, size_t num_samples)
{
    if (!buffer || !tx_chan) return ESP_ERR_INVALID_ARG;

    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(tx_chan, buffer, num_samples * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_write failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

