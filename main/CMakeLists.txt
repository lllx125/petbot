idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS ""
    REQUIRES 
        battery
        camera
        microphone
        imu
        wifi
        arduino
        nvs_flash
        freertos
        driver
)
