/**
 * @file captive_dns.cpp
 * @brief Wildcard DNS server for captive portal (SoftAP mode)
 *
 * Мінімальний DNS-сервер: на будь-який запит відповідає A-записом з IP
 * AP-інтерфейсу (192.168.4.1). Реалізація за зразком ESP-IDF
 * examples/protocols/http_server/captive_portal/components/dns_server.
 */

#include "modesp/net/captive_dns.h"

#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/sockets.h"

static const char* TAG = "captive_dns";

#define DNS_PORT          53
#define DNS_MAX_LEN       256
#define DNS_TTL_SECONDS   60

// DNS header flags (host byte order before htons)
#define DNS_QR_RESPONSE   0x8000  // QR=1 (response)
#define DNS_AA_FLAG       0x0400  // Authoritative Answer
#define DNS_RCODE_OK      0x0000

#define DNS_QTYPE_A       0x0001
#define DNS_QCLASS_IN     0x0001

namespace {

volatile bool   s_running    = false;
volatile bool   s_should_run = false;
int             s_socket     = -1;
TaskHandle_t    s_task       = nullptr;

#pragma pack(push, 1)
struct DnsHeader {
    uint16_t id;
    uint16_t flags;
    uint16_t qd_count;  // questions
    uint16_t an_count;  // answers
    uint16_t ns_count;  // authority
    uint16_t ar_count;  // additional
};
#pragma pack(pop)

// Поточний IP AP-інтерфейсу (fallback 192.168.4.1).
uint32_t ap_ip_addr() {
    esp_netif_t* ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_ip_info_t ip_info;
    if (ap && esp_netif_get_ip_info(ap, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
        return ip_info.ip.addr;  // вже в network byte order
    }
    esp_ip4_addr_t fallback;
    esp_netif_str_to_ip4("192.168.4.1", &fallback);
    return fallback.addr;
}

// Будує DNS-відповідь у `resp` на основі запиту `req` довжиною `req_len`.
// Повертає довжину відповіді або 0, якщо запит відкинуто.
int build_response(const uint8_t* req, int req_len, uint8_t* resp, uint32_t ip) {
    if (req_len < (int)sizeof(DnsHeader) || req_len > DNS_MAX_LEN) return 0;

    const DnsHeader* qhdr = reinterpret_cast<const DnsHeader*>(req);
    // Лише стандартні запити з принаймні одним питанням.
    if (ntohs(qhdr->qd_count) == 0) return 0;
    if (qhdr->flags & htons(DNS_QR_RESPONSE)) return 0;  // це вже відповідь

    // Знайти кінець секції питання (QNAME + QTYPE(2) + QCLASS(2)).
    int pos = sizeof(DnsHeader);
    while (pos < req_len && req[pos] != 0) {
        pos += req[pos] + 1;          // пропустити label
        if (pos >= req_len) return 0; // некоректний пакет
    }
    pos += 1;                          // нульовий байт кінця QNAME
    int qtype_pos = pos;
    pos += 4;                          // QTYPE + QCLASS
    if (pos > req_len) return 0;

    int question_len = pos - (int)sizeof(DnsHeader);

    // Header (echo id) + flags.
    DnsHeader* rhdr = reinterpret_cast<DnsHeader*>(resp);
    rhdr->id       = qhdr->id;
    rhdr->flags    = htons(DNS_QR_RESPONSE | DNS_AA_FLAG | DNS_RCODE_OK);
    rhdr->qd_count = htons(1);
    rhdr->ns_count = 0;
    rhdr->ar_count = 0;

    int out = sizeof(DnsHeader);
    // Копіюємо оригінальну секцію питання.
    memcpy(resp + out, req + sizeof(DnsHeader), question_len);
    out += question_len;

    // Перевіряємо QTYPE: відповідаємо A-записом лише на A-запити;
    // для решти (AAAA тощо) повертаємо лише питання без відповіді,
    // щоб клієнт не вважав, що домен має IPv6/інші записи.
    uint16_t qtype = (uint16_t)((req[qtype_pos] << 8) | req[qtype_pos + 1]);
    if (qtype != DNS_QTYPE_A) {
        rhdr->an_count = 0;
        return out;
    }

    rhdr->an_count = htons(1);

    // Answer: name pointer (0xC00C → зсув 12 = початок питання), TYPE, CLASS, TTL, RDLENGTH, RDATA.
    uint8_t* a = resp + out;
    a[0] = 0xC0; a[1] = 0x0C;                          // compressed name → offset 12
    a[2] = (DNS_QTYPE_A >> 8);  a[3] = (DNS_QTYPE_A & 0xFF);
    a[4] = (DNS_QCLASS_IN >> 8); a[5] = (DNS_QCLASS_IN & 0xFF);
    a[6] = 0; a[7] = 0;                                 // TTL hi
    a[8] = (DNS_TTL_SECONDS >> 8); a[9] = (DNS_TTL_SECONDS & 0xFF);
    a[10] = 0; a[11] = 4;                               // RDLENGTH = 4 (IPv4)
    memcpy(a + 12, &ip, 4);                             // RDATA (network byte order)
    out += 16;

    return out;
}

void dns_task(void*) {
    uint8_t rx[DNS_MAX_LEN];
    uint8_t tx[DNS_MAX_LEN + 16];

    ESP_LOGI(TAG, "DNS server task started (UDP:%d)", DNS_PORT);

    while (s_should_run) {
        struct sockaddr_in src;
        socklen_t src_len = sizeof(src);
        int len = recvfrom(s_socket, rx, sizeof(rx), 0,
                           reinterpret_cast<struct sockaddr*>(&src), &src_len);
        if (len < 0) {
            // timeout (EAGAIN) — перевіряємо прапорець і повторюємо;
            // інші помилки (сокет закрито на stop) — виходимо.
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
            if (!s_should_run) break;
            ESP_LOGW(TAG, "recvfrom error: errno=%d", errno);
            break;
        }

        uint32_t ip = ap_ip_addr();
        int resp_len = build_response(rx, len, tx, ip);
        if (resp_len > 0) {
            sendto(s_socket, tx, resp_len, 0,
                   reinterpret_cast<struct sockaddr*>(&src), src_len);
        }
    }

    ESP_LOGI(TAG, "DNS server task stopped");
    s_running = false;
    s_task = nullptr;
    vTaskDelete(nullptr);
}

} // namespace

void captive_dns_start(void) {
    if (s_running) return;

    s_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_socket < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno=%d", errno);
        return;
    }

    struct sockaddr_in addr = {};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(DNS_PORT);
    if (bind(s_socket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind UDP:%d errno=%d", DNS_PORT, errno);
        close(s_socket);
        s_socket = -1;
        return;
    }

    // Recv timeout → задача може періодично перевіряти s_should_run і коректно вийти.
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(s_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    s_should_run = true;
    s_running    = true;
    if (xTaskCreate(dns_task, "captive_dns", 3072, nullptr, 5, &s_task) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create DNS task");
        s_should_run = false;
        s_running    = false;
        close(s_socket);
        s_socket = -1;
        return;
    }

    ESP_LOGI(TAG, "Captive DNS started on UDP:%d", DNS_PORT);
}

void captive_dns_stop(void) {
    if (!s_running && s_socket < 0) return;

    s_should_run = false;

    if (s_socket >= 0) {
        // shutdown → розблоковує recvfrom, що чекає в задачі.
        shutdown(s_socket, SHUT_RDWR);
        close(s_socket);
        s_socket = -1;
    }

    // Дочекатись завершення задачі (вона сама викликає vTaskDelete).
    for (int i = 0; i < 50 && s_running; ++i) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(TAG, "Captive DNS stopped");
}
