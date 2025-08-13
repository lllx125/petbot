# idf_component_register(
#     SRCS "src/camera.c"
#     INCLUDE_DIRS "include"
# )

set(srcs "src/camera.c"
	     "src/connect_wifi.c")
idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "include"
                    REQUIRES 
                        esp32-camera
                        esp_http_server
                        esp_timer
                        nvs_flash
                        esp_wifi
                        esp_event
                    )
