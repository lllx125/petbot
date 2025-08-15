idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS ""
    REQUIRES 
        camera
        microphone
        imu
        wifi
        arduino
        nvs_flash
        freertos
        driver
)
