#include "imu.h"
#include "driver/i2c.h" //for I2C communication
#include "esp_log.h"
#include <string.h>

// define which ESP32 pins will be used for the I2C bus:
#define I2C_MASTER_SCL_IO 19 // SCL GPIO
#define I2C_MASTER_SDA_IO 20 // SDA GPIO

// Specifying the ESP32 I2C hardware controllers (ESP32 has I2C_NUM_0 and I2C_NUM_1).
// here we use I2C_NUM_0, which is the default I2C controller.
#define I2C_MASTER_NUM I2C_NUM_0

// Sets I2C bus speed to 400kHz (fast mode compared to normal 100kHz) since the LSM6DS3 IMU supports it.
#define I2C_MASTER_FREQ_HZ 400000

// ESP-IDF’s I2C driver can use DMA buffers for sending/receiving,
// but for most simple IMU reads we disable them (set to 0) because they’re unnecessary and take RAM.
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

// I2C address of the LSM6DS3 IMU chip.
#define LSM6DS3_ADDR 0x6A // or 0x6B depending on SA0

// WHO_AM_I_REG is a register in the IMU that always stores a fixed ID number identifying the chip.
// For LSM6DS3, it should always read 0x69.
// We use this to check if the IMU is present and working.
#define WHO_AM_I_REG 0x0F
#define WHO_AM_I_EXPECTED 0x69

// configuration registers
// CTRL1_XL → accelerometer settings
// CTRL2_G → gyroscope settings
#define CTRL1_XL 0x10
#define CTRL2_G 0x11

// starting register addresses for reading data
// OUTX_L_G → gyroscope data
// OUTX_L_XL → accelerometer data
// read from these addresses (and the following registers) to get raw X/Y/Z sensor values.
#define OUTX_L_G 0x22
#define OUTX_L_XL 0x28

// Tag for logging
static const char *TAG = "IMU";

static bool initialized = false;

/**
 * I2C write helper
 * This function writes a single byte of data to a specified register of the LSM6DS3 IMU.
 * It constructs a buffer with the register address and data, then uses the I2C master write function to send it.
 */

static esp_err_t i2c_write(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, LSM6DS3_ADDR,
                                      buf, 2, pdMS_TO_TICKS(100));
}

/**
 * I2C read helper
 * This function reads a specified number of bytes from a given register of the LSM6DS3 IMU.
 * It uses the I2C master write-read function to first write the register address and then read the data.
 */
static esp_err_t i2c_read(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, LSM6DS3_ADDR,
                                        &reg, 1, data, len, pdMS_TO_TICKS(100));
}

/**
 * Configure I²C pins, speed, install driver
 * Read WHO_AM_I register to check if device is connected
 * enable accel & gyro (CTRL1_XL, CTRL2_G)
 * Returns ESP_OK on success, or an error code on failure.
 */

esp_err_t imu_init(void)
{
    esp_err_t ret;

    // Configure I2C bus
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ};
    ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK)
        return ret;

    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                             I2C_MASTER_RX_BUF_DISABLE,
                             I2C_MASTER_TX_BUF_DISABLE, 0);
    if (ret != ESP_OK)
        return ret;

    // WHO_AM_I check
    uint8_t whoami;
    ret = i2c_read(WHO_AM_I_REG, &whoami, 1);
    if (ret != ESP_OK)
        return ret;

    if (whoami != WHO_AM_I_EXPECTED)
    {
        ESP_LOGE(TAG, "LSM6DS3 not found! WHO_AM_I=0x%02X", whoami);
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "LSM6DS3 detected, WHO_AM_I=0x%02X", whoami);

    // Configure accelerometer: 104 Hz, ±2g
    i2c_write(CTRL1_XL, 0x40);
    // Configure gyroscope: 104 Hz, ±245 dps
    i2c_write(CTRL2_G, 0x40);

    initialized = true;
    return ESP_OK;
}

/**
 * Read 6 bytes from gyro starting at OUTX_L_G
 * Read 6 bytes from accel starting at OUTX_L_XL
 * Converts the raw data into physical units (dps for gyro, m/s² for accel).
 * Store them into data->accelerometer and data->gyroscope
 * Returns ESP_OK on success, or an error code on failure.
 */

esp_err_t imu_read_data(imu_data_t *data)
{
    if (!initialized)
        return ESP_ERR_INVALID_STATE;
    if (!data)
        return ESP_ERR_INVALID_ARG;

    uint8_t raw[12];

    // Read gyro (6 bytes)
    esp_err_t ret = i2c_read(OUTX_L_G, raw, 6);
    if (ret != ESP_OK)
        return ret;

    // Read accel (6 bytes)
    ret = i2c_read(OUTX_L_XL, raw + 6, 6);
    if (ret != ESP_OK)
        return ret;

    int16_t gx = (int16_t)(raw[1] << 8 | raw[0]);
    int16_t gy = (int16_t)(raw[3] << 8 | raw[2]);
    int16_t gz = (int16_t)(raw[5] << 8 | raw[4]);

    int16_t ax = (int16_t)(raw[7] << 8 | raw[6]);
    int16_t ay = (int16_t)(raw[9] << 8 | raw[8]);
    int16_t az = (int16_t)(raw[11] << 8 | raw[10]);

    // Convert to physical units
    data->gyroscope.x = gx * (245.0f / 32768.0f); // dps
    data->gyroscope.y = gy * (245.0f / 32768.0f);
    data->gyroscope.z = gz * (245.0f / 32768.0f);

    data->accelerometer.x = ax * (2.0f / 32768.0f) * 9.80665f; // m/s²
    data->accelerometer.y = ay * (2.0f / 32768.0f) * 9.80665f;
    data->accelerometer.z = az * (2.0f / 32768.0f) * 9.80665f;

    return ESP_OK;
}

esp_err_t imu_calibrate(void)
{
    if (!initialized)
        return ESP_ERR_INVALID_STATE;
    // Optional: implement offset calibration
    return ESP_OK;
}

bool imu_is_initialized(void)
{
    return initialized;
}