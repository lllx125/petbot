idf_component_register(
    SRCS "src/battery.c"
    INCLUDE_DIRS "includes"
    REQUIRES driver esp_adc
)