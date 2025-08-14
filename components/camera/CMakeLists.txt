idf_component_register(
    SRCS "src/camera.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server esp32-camera
)
