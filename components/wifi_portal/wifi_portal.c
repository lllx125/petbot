// A minimal Wi-Fi provisioner component for ESP-IDF 5.x
// - Tries STA with creds from NVS
// - Falls back to SoftAP + HTTP form at http://192.168.4.1/ to collect SSID/PASS
// - Saves creds to NVS and connects, then stops portal
//
// Security note: this simple portal uses HTTP over a local SoftAP.
// For production, consider BLE provisioning or HTTPS + a random AP password.

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_check.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_http_server.h"

static const char *TAG = "wifi_portal";

#define WIFI_NVS_NS "wifi"
#define WIFI_NVS_SSID "ssid"
#define WIFI_NVS_PASS "pass"

#define WIFI_MAX_RETRY_DEFAULT 5

// Event bits
#define WIFI_STA_CONNECTED_BIT BIT0
#define WIFI_STA_FAILED_BIT BIT1
#define WIFI_PORTAL_ACTIVE_BIT BIT2

static EventGroupHandle_t s_evt;
static httpd_handle_t s_http = NULL;
static int s_retry_limit = WIFI_MAX_RETRY_DEFAULT;
static char s_ap_pass[64] = "";         // empty => open AP
static char s_ap_prefix[16] = "Petbot"; // AP SSID prefix
static esp_netif_t *s_netif_sta = NULL;
static esp_netif_t *s_netif_ap = NULL;
static bool s_connected = false;

// ------------------------- Utilities -------------------------

static esp_err_t nvs_read_str(nvs_handle_t h, const char *key, char *buf, size_t buflen, bool *found)
{
    size_t len = buflen;
    esp_err_t err = nvs_get_str(h, key, buf, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        *found = false;
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(err, TAG, "nvs_get_str(%s) failed", key);
    *found = true;
    return ESP_OK;
}

static void url_decode_inplace(char *s)
{
    // Very small x-www-form-urlencoded decoder: '+' -> ' ', %xx hex decoded.
    char *w = s;
    for (char *r = s; *r; r++)
    {
        if (*r == '+')
        {
            *w++ = ' ';
        }
        else if (*r == '%' && isxdigit((unsigned char)r[1]) && isxdigit((unsigned char)r[2]))
        {
            char hex[3] = {r[1], r[2], 0};
            *w++ = (char)strtol(hex, NULL, 16);
            r += 2;
        }
        else
        {
            *w++ = *r;
        }
    }
    *w = 0;
}

// ------------------------- HTTP portal -------------------------

// Tiny single-file HTML (inline) for the config page
static const char *INDEX_HTML =
    "<!doctype html><meta charset=utf-8>"
    "<style>body{font:16px system-ui;margin:2rem}label{display:block;margin:.5rem 0}input{font:inherit;padding:.3rem}</style>"
    "<h2>Connect Petbot to Wi-Fi</h2>"
    "<form method=POST action=/save>"
    "<label>Wi-Fi SSID <input name=ssid required></label>"
    "<label>Password <input name=pass type=password></label>"
    "<button type=submit>Save & Connect</button>"
    "</form>"
    "<p>After saving, reconnect your computer/phone to the same Wi-Fi you entered. "
    "Watch the device’s serial log for its IP address.</p>";

static esp_err_t root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t parse_form(char *body, char *out_ssid, size_t ssid_sz, char *out_pass, size_t pass_sz)
{
    // Expect body like: ssid=My+WiFi&pass=secret
    // We decode URL-encoding and split by '&'
    char *ssid = NULL, *pass = NULL;

    // Tokenize on '&'
    for (char *tok = strtok(body, "&"); tok; tok = strtok(NULL, "&"))
    {
        char *eq = strchr(tok, '=');
        if (!eq)
            continue;
        *eq = 0;
        char *k = tok, *v = eq + 1;
        url_decode_inplace(k);
        url_decode_inplace(v);
        if (strcmp(k, "ssid") == 0)
            ssid = v;
        else if (strcmp(k, "pass") == 0)
            pass = v;
    }

    if (!ssid || strlen(ssid) == 0)
        return ESP_FAIL;
    snprintf(out_ssid, ssid_sz, "%s", ssid);
    snprintf(out_pass, pass_sz, "%s", pass ? pass : "");
    return ESP_OK;
}

static esp_err_t save_post(httpd_req_t *req)
{
    // Read POST body (small)
    char buf[256];
    int total = req->content_len;
    if (total <= 0 || total >= (int)sizeof(buf))
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body too large");
    int got = httpd_req_recv(req, buf, total);
    if (got <= 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad body");
    buf[got] = 0;

    char ssid[33] = {0}, pass[65] = {0};
    if (parse_form(buf, ssid, sizeof(ssid), pass, sizeof(pass)) != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid");
    }

    // Store to NVS
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open(WIFI_NVS_NS, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_str(nvs, WIFI_NVS_SSID, ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs, WIFI_NVS_PASS, pass));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);

    ESP_LOGI(TAG, "Saved Wi-Fi creds: ssid='%s' (pass %s)", ssid, strlen(pass) ? "set" : "empty");

    // Respond and trigger STA connect on a separate task to avoid blocking HTTP
    const char *ok =
        "<h3>Saved!</h3><p>The device will try to connect now.</p>"
        "<p>Reconnect your phone/PC to that network and check the serial log for the IP.</p>";
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, ok, HTTPD_RESP_USE_STRLEN);

    // Defer connect to a task
    xEventGroupSetBits(s_evt, WIFI_STA_FAILED_BIT); // signal portal loop to switch to STA
    return ESP_OK;
}

static void start_http(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&s_http, &cfg));

    httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = root_get};
    httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = save_post};
    httpd_register_uri_handler(s_http, &root);
    httpd_register_uri_handler(s_http, &save);

    ESP_LOGI(TAG, "Portal at http://192.168.4.1/");
}

static void stop_http(void)
{
    if (s_http)
    {
        httpd_stop(s_http);
        s_http = NULL;
    }
}

// ------------------------- Wi-Fi core -------------------------

static void wifi_events(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
    {
        static int tries = 0;
        if (tries++ < s_retry_limit)
        {
            ESP_LOGW(TAG, "STA disconnected, retry %d/%d", tries, s_retry_limit);
            esp_wifi_connect();
        }
        else
        {
            ESP_LOGE(TAG, "STA connect failed");
            xEventGroupSetBits(s_evt, WIFI_STA_FAILED_BIT);
            tries = 0; // reset for next attempt
        }
    }
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        s_connected = true;
        xEventGroupSetBits(s_evt, WIFI_STA_CONNECTED_BIT);
    }
}

static void start_sta_from_nvs(void)
{
    // Try to open NVS. If the wifi namespace doesn't exist yet, just fall back to portal.
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(WIFI_NVS_NS, NVS_READONLY, &nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "NVS namespace '%s' not found (first boot) → portal", WIFI_NVS_NS);
        xEventGroupSetBits(s_evt, WIFI_STA_FAILED_BIT);
        return;
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        xEventGroupSetBits(s_evt, WIFI_STA_FAILED_BIT);
        return;
    }

    // Read SSID/PASS (SSID must exist; PASS may be empty)
    char ssid[33] = {0}, pass[65] = {0};
    size_t len;
    bool found = false;

    len = sizeof(ssid);
    err = nvs_get_str(nvs, WIFI_NVS_SSID, ssid, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND || ssid[0] == '\0')
    {
        ESP_LOGW(TAG, "No saved SSID in NVS → portal");
        nvs_close(nvs);
        xEventGroupSetBits(s_evt, WIFI_STA_FAILED_BIT);
        return;
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_get_str(SSID) failed: %s", esp_err_to_name(err));
        nvs_close(nvs);
        xEventGroupSetBits(s_evt, WIFI_STA_FAILED_BIT);
        return;
    }

    len = sizeof(pass);
    err = nvs_get_str(nvs, WIFI_NVS_PASS, pass, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND)
        pass[0] = '\0'; // empty password is fine
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "nvs_get_str(PASS) failed: %s", esp_err_to_name(err));
        nvs_close(nvs);
        xEventGroupSetBits(s_evt, WIFI_STA_FAILED_BIT);
        return;
    }
    nvs_close(nvs);

    // Bring up STA with the loaded creds
    if (!s_netif_sta)
        s_netif_sta = esp_netif_create_default_wifi_sta();

    wifi_config_t cfg = {0};
    strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
    strncpy((char *)cfg.sta.password, pass, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    cfg.sta.pmf_cfg.capable = true;
    cfg.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to SSID '%s' ...", ssid);
}

// Start a SoftAP and HTTP portal for entering SSID/PASS.
static void start_portal(void)
{
    // Bring up AP interface
    if (!s_netif_ap)
        s_netif_ap = esp_netif_create_default_wifi_ap();

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "%s-%02X%02X", s_ap_prefix, mac[4], mac[5]);

    wifi_config_t ap = {0};
    strncpy((char *)ap.ap.ssid, ssid, sizeof(ap.ap.ssid));
    ap.ap.ssid_len = strlen(ssid);
    ap.ap.channel = 1;
    ap.ap.max_connection = 4;
    if (strlen(s_ap_pass))
    {
        strncpy((char *)ap.ap.password, s_ap_pass, sizeof(ap.ap.password));
        ap.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    }
    else
    {
        ap.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Started setup AP '%s' (password: %s)", ssid, strlen(s_ap_pass) ? s_ap_pass : "(open)");
    start_http();
    xEventGroupSetBits(s_evt, WIFI_PORTAL_ACTIVE_BIT);
}

static void stop_portal(void)
{
    stop_http();
    if (s_netif_ap)
    {
        ESP_ERROR_CHECK(esp_wifi_stop());
        // keep netif; switching to STA will reuse Wi-Fi driver start
    }
    xEventGroupClearBits(s_evt, WIFI_PORTAL_ACTIVE_BIT);
}

// ------------------------- Public API -------------------------

void wifi_portal_set_sta_retry_limit(int retries) { s_retry_limit = retries > 0 ? retries : WIFI_MAX_RETRY_DEFAULT; }
void wifi_portal_set_ap_password(const char *ap_pass)
{
    if (!ap_pass)
    {
        s_ap_pass[0] = 0;
        return;
    }
    snprintf(s_ap_pass, sizeof(s_ap_pass), "%s", ap_pass);
}
void wifi_portal_set_ap_ssid_prefix(const char *prefix)
{
    if (!prefix || !*prefix)
        return;
    snprintf(s_ap_prefix, sizeof(s_ap_prefix), "%s", prefix);
}

void wifi_portal_erase_credentials(void)
{
    nvs_handle_t nvs;
    if (nvs_open(WIFI_NVS_NS, NVS_READWRITE, &nvs) == ESP_OK)
    {
        nvs_erase_key(nvs, WIFI_NVS_SSID);
        nvs_erase_key(nvs, WIFI_NVS_PASS);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

bool wifi_portal_get_sta_ip(esp_ip4_addr_t *out_ip)
{
    if (!s_connected || !s_netif_sta || !out_ip)
        return false;
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(s_netif_sta, &ip) != ESP_OK)
        return false;
    *out_ip = ip.ip;
    return true;
}

bool wifi_portal_start(void)
{
    // Before: ESP_ERROR_CHECK(nvs_flash_init());
    esp_err_t nvs_ok = nvs_flash_init();
    if (nvs_ok == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ok == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated or changed: erase once and retry
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ok = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ok);

    // One-time core init
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    s_evt = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_events, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_events, NULL, NULL));

    // Try STA from NVS
    start_sta_from_nvs();

    // Wait for either "connected" or "failed => portal"
    EventBits_t bits = xEventGroupWaitBits(s_evt,
                                           WIFI_STA_CONNECTED_BIT | WIFI_STA_FAILED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(15000));

    if (bits & WIFI_STA_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Connected to STA");
        return true;
    }

    // No creds or failed → portal
    ESP_LOGW(TAG, "Starting setup portal");
    start_portal();

    // Keep a small loop: when user submits creds, we set FAILED bit to reattempt STA
    while (!s_connected)
    {
        EventBits_t ev = xEventGroupWaitBits(s_evt, WIFI_STA_FAILED_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        if (ev & WIFI_STA_FAILED_BIT)
        {
            // Stop AP/HTTP, switch to STA with new creds
            stop_portal();
            start_sta_from_nvs();

            // Wait a bit for success; otherwise, return to portal
            EventBits_t ok = xEventGroupWaitBits(s_evt,
                                                 WIFI_STA_CONNECTED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(15000));
            if (ok & WIFI_STA_CONNECTED_BIT)
            {
                ESP_LOGI(TAG, "Connected to STA");
                break;
            }
            else
            {
                ESP_LOGW(TAG, "STA connect failed; returning to portal");
                start_portal();
            }
        }
    }
    return s_connected;
}
