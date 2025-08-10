#include "web_server.h"
#include "storage.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include <stdarg.h>
#include <string.h>

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

static const char *TAG = "web";

// ======= In‑memory log ring (very small & simple) =======
#define LOG_MAX 256
static char logbuf[LOG_MAX][160];
static uint32_t log_seq[LOG_MAX];
static volatile uint32_t log_head = 0;   // next write slot
static volatile uint32_t global_seq = 0; // monotonically increases

void petbot_log_push_line(const char *line)
{
    size_t slot = log_head % LOG_MAX;
    snprintf(logbuf[slot], sizeof(logbuf[slot]), "%s", line);
    log_seq[slot] = ++global_seq;
    log_head++;
}

// ======= Static file helpers (embedded via idf_component_embed_files) =======
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t logs_html_start[] asm("_binary_logs_html_start");
extern const uint8_t logs_html_end[] asm("_binary_logs_html_end");
extern const uint8_t ota_html_start[] asm("_binary_ota_html_start");
extern const uint8_t ota_html_end[] asm("_binary_ota_html_end");
extern const uint8_t settings_html_start[] asm("_binary_settings_html_start");
extern const uint8_t settings_html_end[] asm("_binary_settings_html_end");
extern const uint8_t assets_main_js_start[] asm("_binary_main_js_start");
extern const uint8_t assets_main_js_end[] asm("_binary_main_js_end");
extern const uint8_t assets_style_css_start[] asm("_binary_style_css_start");
extern const uint8_t assets_style_css_end[] asm("_binary_style_css_end");

static esp_err_t send_blob(httpd_req_t *req, const uint8_t *start, const uint8_t *end, const char *type)
{
    httpd_resp_set_type(req, type);
    size_t len = end - start;
    return httpd_resp_send(req, (const char *)start, len);
}

// ======= HTTP Handlers =======
static esp_err_t h_root(httpd_req_t *req) { return send_blob(req, index_html_start, index_html_end, "text/html"); }
static esp_err_t h_logs(httpd_req_t *req) { return send_blob(req, logs_html_start, logs_html_end, "text/html"); }
static esp_err_t h_ota(httpd_req_t *req) { return send_blob(req, ota_html_start, ota_html_end, "text/html"); }
static esp_err_t h_settings(httpd_req_t *req) { return send_blob(req, settings_html_start, settings_html_end, "text/html"); }
static esp_err_t h_js(httpd_req_t *req) { return send_blob(req, assets_main_js_start, assets_main_js_end, "application/javascript"); }
static esp_err_t h_css(httpd_req_t *req) { return send_blob(req, assets_style_css_start, assets_style_css_end, "text/css"); }

// GET /api/log?since=N  -> JSON array of {seq, line}
static esp_err_t h_api_log(httpd_req_t *req)
{
    char q[32];
    int since = 0;
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK)
    {
        char val[16];
        if (httpd_query_key_value(q, "since", val, sizeof(val)) == ESP_OK)
        {
            since = atoi(val);
        }
    }

    cJSON *arr = cJSON_CreateArray();
    uint32_t head = log_head;
    uint32_t start = (head > LOG_MAX ? head - LOG_MAX : 0);
    for (uint32_t i = start; i < head; ++i)
    {
        size_t slot = i % LOG_MAX;
        if ((int)log_seq[slot] > since)
        {
            cJSON *o = cJSON_CreateObject();
            cJSON_AddNumberToObject(o, "seq", log_seq[slot]);
            cJSON_AddStringToObject(o, "line", logbuf[slot]);
            cJSON_AddItemToArray(arr, o);
        }
    }
    char *out = cJSON_PrintUnformatted(arr);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out);
    cJSON_Delete(arr);
    free(out);
    return ESP_OK;
}

// Wi‑Fi scan: GET /api/wifi/scan -> [{ssid,rssi,auth}]
static esp_err_t h_api_scan(httpd_req_t *req)
{
    wifi_scan_config_t sc = {0};
    esp_wifi_scan_start(&sc, true);
    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    wifi_ap_record_t *recs = calloc(n, sizeof(*recs));
    esp_wifi_scan_get_ap_records(&n, recs);

    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < n; ++i)
    {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "ssid", (const char *)recs[i].ssid);
        cJSON_AddNumberToObject(o, "rssi", recs[i].rssi);
        cJSON_AddNumberToObject(o, "auth", recs[i].authmode);
        cJSON_AddItemToArray(arr, o);
    }
    free(recs);
    char *out = cJSON_PrintUnformatted(arr);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out);
    cJSON_Delete(arr);
    free(out);
    return ESP_OK;
}

// Saved creds: GET /api/wifi/saved -> [{ssid}]
static esp_err_t h_api_saved(httpd_req_t *req)
{
    wifi_cred_t *arr = NULL;
    size_t count = 0;
    storage_load_creds(&arr, &count);
    cJSON *ja = cJSON_CreateArray();
    for (size_t i = 0; i < count; ++i)
    {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "ssid", arr[i].ssid);
        cJSON_AddBoolToObject(o, "hasPass", strlen(arr[i].pass) > 0);
        cJSON_AddItemToArray(ja, o);
    }
    free(arr);
    char *out = cJSON_PrintUnformatted(ja);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out);
    cJSON_Delete(ja);
    free(out);
    return ESP_OK;
}

// Save credential: POST /api/wifi/save  {ssid, password}
static esp_err_t h_api_save(httpd_req_t *req)
{
    char buf[256];
    int r = httpd_req_recv(req, buf, MIN(sizeof(buf) - 1, req->content_len));
    if (r <= 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "no body");
    buf[r] = 0;
    cJSON *j = cJSON_Parse(buf);
    wifi_cred_t c = {0};
    snprintf(c.ssid, sizeof(c.ssid), "%s", cJSON_GetObjectItem(j, "ssid")->valuestring);
    snprintf(c.pass, sizeof(c.pass), "%s", cJSON_GetObjectItem(j, "password")->valuestring);
    cJSON_Delete(j);
    esp_err_t e = storage_save_or_update(&c);
    if (e == ESP_OK)
    {
        httpd_resp_sendstr(req, "OK");
    }
    else
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "save failed");
    }
    return ESP_OK;
}

// Delete credential: DELETE /api/wifi/delete?ssid=...
static esp_err_t h_api_delete(httpd_req_t *req)
{
    char q[64];
    char ssid[33] = {0};
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK)
    {
        httpd_query_key_value(q, "ssid", ssid, sizeof(ssid));
    }
    if (ssid[0] == 0)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ssid?");
    esp_err_t e = storage_delete_ssid(ssid);
    if (e == ESP_OK)
        httpd_resp_sendstr(req, "OK");
    else
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "not found");
    return ESP_OK;
}

// 302 redirect helper used by captive portal to force browser to petbot.com
static esp_err_t h_redirect_root(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://petbot.com/");
    return httpd_resp_send(req, NULL, 0);
}

// Captive portal OS checks
static esp_err_t h_gen204(httpd_req_t *req) { return h_redirect_root(req); }  // /generate_204
static esp_err_t h_hotspot(httpd_req_t *req) { return h_redirect_root(req); } // /hotspot-detect.html

static const httpd_uri_t routes[] = {
    {.uri = "/", .method = HTTP_GET, .handler = h_root},
    {.uri = "/logs", .method = HTTP_GET, .handler = h_logs},
    {.uri = "/ota", .method = HTTP_GET, .handler = h_ota},
    {.uri = "/settings", .method = HTTP_GET, .handler = h_settings},
    {.uri = "/assets/main.js", .method = HTTP_GET, .handler = h_js},
    {.uri = "/assets/style.css", .method = HTTP_GET, .handler = h_css},

    {.uri = "/api/log", .method = HTTP_GET, .handler = h_api_log},
    {.uri = "/api/wifi/scan", .method = HTTP_GET, .handler = h_api_scan},
    {.uri = "/api/wifi/saved", .method = HTTP_GET, .handler = h_api_saved},
    {.uri = "/api/wifi/save", .method = HTTP_POST, .handler = h_api_save},
    {.uri = "/api/wifi/delete", .method = HTTP_DELETE, .handler = h_api_delete},

    // Captive portal endpoints
    {.uri = "/generate_204", .method = HTTP_GET, .handler = h_gen204},
    {.uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = h_hotspot},
};

esp_err_t petbot_web_start(httpd_handle_t *server_out)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.max_uri_handlers = 16;

    httpd_handle_t s = NULL;
    ESP_ERROR_CHECK(httpd_start(&s, &cfg));

    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i)
    {
        httpd_register_uri_handler(s, &routes[i]);
    }

    if (server_out)
        *server_out = s;
    ESP_LOGI(TAG, "HTTP server started");
    return ESP_OK;
}

void petbot_web_stop(httpd_handle_t server)
{
    if (server)
        httpd_stop(server);
}