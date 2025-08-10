#include "dns_server.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "dns";
static TaskHandle_t s_task;
static int s_sock = -1;
static uint32_t s_answer_ip_be;

// Very tiny DNS responder: for any A query, respond with s_answer_ip_be
// NOTE: For captive AP only; not a full DNS implementation.

static void dns_task(void *arg)
{
    s_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_sock < 0)
    {
        ESP_LOGE(TAG, "socket() failed");
        vTaskDelete(NULL);
        return;
    }

    int yes = 1;
    setsockopt(s_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in a = {.sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(53)};
    if (bind(s_sock, (struct sockaddr *)&a, sizeof(a)) < 0)
    {
        ESP_LOGE(TAG, "bind() failed");
        vTaskDelete(NULL);
        return;
    }

    // Use static buffers to avoid large stack frames and guard against overflows
    static uint8_t req[512];
    static uint8_t resp[512];

    while (1)
    {
        struct sockaddr_in from;
        socklen_t fl = sizeof(from);
        int r = recvfrom(s_sock, req, sizeof(req), 0, (struct sockaddr *)&from, &fl);
        if (r <= 0)
            continue;
        if (r < 12)
            continue; // invalid DNS header length

        // Copy request to response first (echo header+question)
        int copy_len = r;
        if (copy_len > (int)(sizeof(resp) - 16))
        { // ensure room for one A record answer (16 bytes)
            ESP_LOGW(TAG, "DNS pkt too large (%d), dropping", r);
            continue;
        }
        memcpy(resp, req, copy_len);

        // Set flags: response, recursion not available, no error
        resp[2] = 0x81;
        resp[3] = 0x80;
        // Keep QDCOUNT as-is; set ANCOUNT = 1; NSCOUNT=0; ARCOUNT=0
        resp[6] = 0x00;
        resp[7] = 0x01; // ancount
        resp[8] = resp[9] = resp[10] = resp[11] = 0x00;

        // Find end of QNAME safely
        int i = 12; // start of QNAME
        while (i < r && req[i] != 0)
        {
            int lab = req[i];
            if (lab <= 0 || (i + 1 + lab) >= r)
            {
                i = r;
                break;
            }
            i += 1 + lab;
        }
        if (i >= r)
            continue;          // malformed qname
        int qtail = i + 1 + 4; // null + QTYPE(2) + QCLASS(2)
        if (qtail > r)
            continue; // malformed

        // Append one A answer: name as a pointer to original question at 0x0C
        int p = copy_len;
        if (p + 16 > (int)sizeof(resp))
        { // should be covered by earlier check, but double-guard
            ESP_LOGW(TAG, "DNS resp would overflow, dropping");
            continue;
        }
        resp[p++] = 0xC0;
        resp[p++] = 0x0C; // NAME = pointer to 0x000C
        resp[p++] = 0x00;
        resp[p++] = 0x01; // TYPE = A
        resp[p++] = 0x00;
        resp[p++] = 0x01; // CLASS = IN
        resp[p++] = 0x00;
        resp[p++] = 0x00;
        resp[p++] = 0x00;
        resp[p++] = 0x1E; // TTL = 30s
        resp[p++] = 0x00;
        resp[p++] = 0x04; // RDLENGTH = 4
        memcpy(&resp[p], &s_answer_ip_be, 4);
        p += 4; // RDATA = our AP IP

        sendto(s_sock, resp, p, 0, (struct sockaddr *)&from, fl);
    }
}

esp_err_t dns_server_start(uint32_t ip_be)
{
    s_answer_ip_be = ip_be;
    // Increase stack a bit and give moderate priority; depth is in words (4 bytes each)
    const uint32_t stack_words = 3072; // ~12 KB
    if (xTaskCreate(dns_task, "dns", stack_words, NULL, 4, &s_task) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create DNS task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "DNS captive server started");
    return ESP_OK;
}

void dns_server_stop(void)
{
    if (s_sock >= 0)
        close(s_sock);
    if (s_task)
        vTaskDelete(s_task);
}