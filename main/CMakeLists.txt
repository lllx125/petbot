idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS ""
    REQUIRES 
        camera
        esp32-camera
        imu
        wifi
        arduino
        nvs_flash
        freertos
)
