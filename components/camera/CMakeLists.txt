idf_component_register(
    SRCS "camera.c" "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp32-camera esp_http_server esp_wifi nvs_flash
)
