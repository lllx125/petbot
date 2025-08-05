#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

#define UART_NUM UART_NUM_0
#define BUF_SIZE 1024
#define TAG "PETBOT_ECHO"

void app_main(void)
{
    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    // Install UART driver
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    
    ESP_LOGI(TAG, "Petbot Echo Server Started!");
    ESP_LOGI(TAG, "Send me a message and I'll echo it back!");
    
    uint8_t data[BUF_SIZE];
    
    while (1) {
        // Read data from UART
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, 100 / portTICK_PERIOD_MS);
        
        if (len > 0) {
            // Null-terminate the received data
            data[len] = '\0';
            
            // Log the received message
            ESP_LOGI(TAG, "Received: %s", (char*)data);
            
            // Echo the message back
            uart_write_bytes(UART_NUM, "Echo: ", 6);
            uart_write_bytes(UART_NUM, (const char*)data, len);
            uart_write_bytes(UART_NUM, "\r\n", 2);
        }
        
        // Small delay to prevent watchdog timeout
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}