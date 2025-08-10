#include "wifi.h"
#include "web_server.h"
#include "storage.h"
#include "ota.h"
#include "dns_server.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "mdns.h"
#include <string.h>

static const char *TAG = "wifi";

static EventGroupHandle_t s_wifi_event_group;
static httpd_handle_t s_http = NULL;
static esp_netif_t *s_sta = NULL;
static esp_netif_t *s_ap = NULL;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static petbot_net_cfg_t s_cfg;

// Forward decls
static esp_err_t start_softap(void);
static esp_err_t start_station_try_saved(void);
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

// ---------- Public API ----------

esp_err_t petbot_wifi_start(const petbot_net_cfg_t *cfg)
{
    s_cfg = *cfg;

    // Initializes the TCP/IP network interface layer. This sets up the internal structures needed for network communication.
    ESP_ERROR_CHECK(esp_netif_init());

    // Creates the default event loop. This is required for handling asynchronous events (like Wi-Fi events, IP events, etc.) in the ESP-IDF framework.
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default interfaces (but we bring up only the one we need)
    s_sta = esp_netif_create_default_wifi_sta();
    s_ap = esp_netif_create_default_wifi_ap();

    // Initialize the Wi-Fi driver with default configuration
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &ip_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    // Try STA first using saved creds; fall back to AP
    if (start_station_try_saved() == ESP_OK)
    {
        return ESP_OK;
    }
    ESP_LOGW(TAG, "No saved networks or failed to connect. Starting SoftAP...");
    return start_softap();
}

// Allow other modules to push a log line (used by petbot_web_logf wrapper)
void petbot_web_logf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ESP_LOGI("WEBLOG", "%s", buf);
    petbot_log_push_line(buf);
}

// ---------- Event handlers + STA/AP bring‑up ----------

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "STA disconnected");
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START)
    {
        ESP_LOGI(TAG, "SoftAP started");
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
/**
 * @brief Attempts to start the Wi-Fi station mode using saved credentials.
 *
 * This function sets the Wi-Fi mode to station (WIFI_MODE_STA) and attempts to connect
 * to a previously saved Wi-Fi network. It scans for visible networks and selects the
 * best match based on the received signal strength indicator (RSSI) among the saved
 * credentials. If a connection is successfully established, it initializes mDNS and
 * starts the web server.
 *
 * @return
 *      - ESP_OK on successful connection to a Wi-Fi network.
 *      - ESP_FAIL if no saved credentials are available, no matching networks are
 *        visible, or the connection attempt times out.
 *
 * @note
 * - The function blocks while scanning for networks and waiting for a connection.
 * - The hostname is set to "petbot" for DHCP and mDNS purposes.
 * - If the connection is successful, the device can be accessed via http://petbot.local.
 */

static esp_err_t start_station_try_saved(void)
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Load saved credentials
    wifi_cred_t *arr = NULL;
    size_t count = 0;
    storage_load_creds(&arr, &count);
    if (!arr || count == 0)
    {
        free(arr);
        return ESP_FAIL;
    }

    // Strategy: scan for visible networks and try the best saved match by RSSI
    wifi_scan_config_t sc = {0};
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_scan_start(&sc, true /*block*/));

    uint16_t ap_num = 0;
    esp_wifi_scan_get_ap_num(&ap_num);
    wifi_ap_record_t *aps = calloc(ap_num, sizeof(*aps));
    esp_wifi_scan_get_ap_records(&ap_num, aps);

    int best_idx_saved = -1, best_rssi = -1000;
    for (size_t i = 0; i < count; ++i)
    {
        for (int j = 0; j < ap_num; ++j)
        {
            if (strncmp((const char *)aps[j].ssid, arr[i].ssid, 32) == 0)
            {
                if (aps[j].rssi > best_rssi)
                {
                    best_rssi = aps[j].rssi;
                    best_idx_saved = (int)i;
                }
            }
        }
    }

    free(aps);

    if (best_idx_saved < 0)
    {
        ESP_LOGW(TAG, "Saved SSIDs not visible right now");
        free(arr);
        return ESP_FAIL;
    }

    wifi_config_t wc = {0};
    strncpy((char *)wc.sta.ssid, arr[best_idx_saved].ssid, sizeof(wc.sta.ssid));
    strncpy((char *)wc.sta.password, arr[best_idx_saved].pass, sizeof(wc.sta.password));
    free(arr);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));

    // Set hostname for DHCP + mDNS name
    esp_netif_set_hostname(s_sta, "petbot");

    ESP_ERROR_CHECK(esp_wifi_connect());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));

    if (bits & WIFI_CONNECTED_BIT)
    {
        // mDNS so you can reach http://petbot.local when on your home Wi‑Fi
        mdns_init();
        mdns_hostname_set("petbot");
        mdns_instance_name_set("PetBot");
        mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

        // Start the web server (shared handlers for STA & AP)
        petbot_web_start(&s_http);
        ota_register_handlers(s_http);
        ESP_LOGI(TAG, "STA ready at http://petbot.local");
        return ESP_OK;
    }

    ESP_LOGW(TAG, "STA connect timeout");
    esp_wifi_stop();
    return ESP_FAIL;
}

static esp_err_t start_softap(void)
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t apc = {0};
    snprintf((char *)apc.ap.ssid, sizeof(apc.ap.ssid), "%s", s_cfg.ap_ssid ? s_cfg.ap_ssid : "PetBot-Setup");
    apc.ap.channel = s_cfg.ap_channel ? s_cfg.ap_channel : 6;
    apc.ap.max_connection = 4;
    if (s_cfg.ap_pass && strlen(s_cfg.ap_pass) >= 8)
    {
        snprintf((char *)apc.ap.password, sizeof(apc.ap.password), "%s", s_cfg.ap_pass);
        apc.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    }
    else
    {
        apc.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &apc));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Start HTTP server and force a captive DNS that resolves any host to AP IP
    petbot_web_start(&s_http);
    ota_register_handlers(s_http);

    // Get AP IP in network byte order
    esp_netif_ip_info_t ip;
    esp_netif_get_ip_info(s_ap, &ip);
    uint32_t ip_be = ip.ip.addr; // already in lwIP order
    dns_server_start(ip_be);

    ESP_LOGI(TAG, "AP ready. Join Wi‑Fi '%s' then open http://petbot.com (captive)", (char *)apc.ap.ssid);
    return ESP_OK;
}