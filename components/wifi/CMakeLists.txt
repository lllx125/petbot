idf_build_get_property(project_dir PROJECT_DIR)
set(WEB_DIR "${project_dir}/web")

idf_component_register(
    SRCS
        "src/wifi.c"
        "src/web_server.c"
        "src/storage.c"
        "src/ota.c"
        "src/dns_server.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        esp_wifi esp_netif nvs_flash esp_http_server mdns esp_timer app_update json
    EMBED_FILES
        ${WEB_DIR}/index.html
        ${WEB_DIR}/logs.html
        ${WEB_DIR}/ota.html
        ${WEB_DIR}/settings.html
        ${WEB_DIR}/assets/style.css
        ${WEB_DIR}/assets/main.js
)

