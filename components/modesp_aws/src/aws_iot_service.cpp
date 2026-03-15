/**
 * @file aws_iot_service.cpp
 * @brief AWS IoT Core cloud service — skeleton implementation
 *
 * Phase 3: mTLS connection + telemetry publish.
 * Phase 5: Device Shadow (desired/reported).
 * Phase 6: IoT Jobs (OTA).
 * Phase 7: Fleet Provisioning by Claim.
 */

#include "modesp/net/aws_iot_service.h"
#include "modesp/shared_state.h"
#include "modesp/services/nvs_helper.h"

#include "esp_log.h"
#include "esp_mac.h"
#include "jsmn.h"

#include <cstring>
#include <cstdio>

static const char* TAG = "AwsIoT";

namespace modesp {

// Статичні буфери для сертифікатів (.bss, не heap)
char AwsIotService::s_device_cert_pem_[CERT_BUF_SIZE] = {};
char AwsIotService::s_device_key_pem_[CERT_BUF_SIZE] = {};

// ═══════════════════════════════════════════════════════════════
// BaseModule interface
// ═══════════════════════════════════════════════════════════════

bool AwsIotService::on_init() {
    // Генеруємо device_id з MAC (аналогічно MqttService)
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(device_id_, sizeof(device_id_), "%02X%02X%02X",
             mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "Device ID: %s", device_id_);

    // Завантажуємо конфігурацію з NVS
    load_config();

    // Завантажуємо сертифікати
    cert_loaded_ = load_certificates();

    if (enabled_ && endpoint_[0] != '\0' && cert_loaded_) {
        ESP_LOGI(TAG, "AWS IoT Core endpoint: %s, thing: %s",
                 endpoint_, thing_name_);
        // TODO Phase 3: coreMQTT connect з mTLS
        ESP_LOGW(TAG, "coreMQTT connection not implemented yet (skeleton)");
    } else {
        if (!enabled_) {
            ESP_LOGI(TAG, "AWS IoT Core disabled");
        } else if (endpoint_[0] == '\0') {
            ESP_LOGW(TAG, "No endpoint configured — set via /api/cloud");
        } else if (!cert_loaded_) {
            ESP_LOGW(TAG, "No certificates loaded — upload via /api/cloud");
        }
    }

    return true;
}

void AwsIotService::on_update(uint32_t dt_ms) {
    if (!enabled_ || !connected_) return;

    // TODO Phase 3: delta-publish telemetry via coreMQTT
    // TODO Phase 5: Device Shadow sync
    // TODO Phase 6: IoT Jobs polling
    (void)dt_ms;
}

void AwsIotService::on_stop() {
    // TODO Phase 3: disconnect coreMQTT
    connected_ = false;
    ESP_LOGI(TAG, "AWS IoT Core service stopped");
}

// ═══════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════

void AwsIotService::load_config() {
    nvs_helper::read_str("awscert", "endpoint", endpoint_, sizeof(endpoint_));
    nvs_helper::read_str("awscert", "thing", thing_name_, sizeof(thing_name_));
    nvs_helper::read_bool("awscert", "enabled", enabled_);

    // Якщо thing_name не заданий — генеруємо з device_id
    if (thing_name_[0] == '\0') {
        snprintf(thing_name_, sizeof(thing_name_), "modesp-%s", device_id_);
    }
}

bool AwsIotService::load_certificates() {
    size_t cert_len = 0, key_len = 0;

    bool cert_ok = nvs_helper::read_blob("awscert", "cert",
                                          s_device_cert_pem_, CERT_BUF_SIZE - 1, cert_len);
    bool key_ok = nvs_helper::read_blob("awscert", "key",
                                         s_device_key_pem_, CERT_BUF_SIZE - 1, key_len);

    if (cert_ok && key_ok && cert_len > 0 && key_len > 0) {
        // Null-terminate PEM strings
        s_device_cert_pem_[cert_len] = '\0';
        s_device_key_pem_[key_len] = '\0';
        ESP_LOGI(TAG, "Certificates loaded: cert=%u bytes, key=%u bytes",
                 (unsigned)cert_len, (unsigned)key_len);
        return true;
    }

    ESP_LOGD(TAG, "No certificates in NVS");
    return false;
}

// ═══════════════════════════════════════════════════════════════
// HTTP handlers
// ═══════════════════════════════════════════════════════════════

void AwsIotService::set_http_server(httpd_handle_t server) {
    server_ = server;
    if (server_ && !http_registered_) {
        register_http_handlers();
        http_registered_ = true;
    }
}

void AwsIotService::set_cors_headers(httpd_req_t* req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Authorization");
}

esp_err_t AwsIotService::handle_get_cloud(httpd_req_t* req) {
    auto* self = static_cast<AwsIotService*>(req->user_ctx);
    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");

    char buf[384];
    int len = snprintf(buf, sizeof(buf),
        "{\"provider\":\"aws\","
        "\"endpoint\":\"%s\","
        "\"thing_name\":\"%s\","
        "\"device_id\":\"%s\","
        "\"enabled\":%s,"
        "\"connected\":%s,"
        "\"cert_loaded\":%s}",
        self->endpoint_,
        self->thing_name_,
        self->device_id_,
        self->enabled_ ? "true" : "false",
        self->connected_ ? "true" : "false",
        self->cert_loaded_ ? "true" : "false");

    return httpd_resp_send(req, buf, len);
}

esp_err_t AwsIotService::handle_post_cloud(httpd_req_t* req) {
    auto* self = static_cast<AwsIotService*>(req->user_ctx);
    set_cors_headers(req);

    // Читаємо body (до 4KB — сертифікати можуть бути великими)
    static char body[4096];
    int received = httpd_req_recv(req, body, sizeof(body) - 1);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }
    body[received] = '\0';

    ESP_LOGI(TAG, "POST /api/cloud: %d bytes", received);

    // Parse JSON з jsmn
    jsmn_parser parser;
    jsmntok_t tokens[32];
    jsmn_init(&parser);
    int r = jsmn_parse(&parser, body, received, tokens, 32);
    if (r < 2 || tokens[0].type != JSMN_OBJECT) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    bool config_changed = false;
    bool cert_uploaded = false;

    for (int i = 1; i < r - 1; i += 2) {
        if (tokens[i].type != JSMN_STRING) continue;

        int klen = tokens[i].end - tokens[i].start;
        int vlen = tokens[i + 1].end - tokens[i + 1].start;
        const char* k = body + tokens[i].start;
        const char* v = body + tokens[i + 1].start;

        // Null-terminate value тимчасово
        char saved = body[tokens[i + 1].end];
        body[tokens[i + 1].end] = '\0';

        if (klen == 8 && strncmp(k, "endpoint", 8) == 0) {
            strncpy(self->endpoint_, v, sizeof(self->endpoint_) - 1);
            self->endpoint_[sizeof(self->endpoint_) - 1] = '\0';
            nvs_helper::write_str("awscert", "endpoint", self->endpoint_);
            config_changed = true;
        } else if (klen == 10 && strncmp(k, "thing_name", 10) == 0) {
            strncpy(self->thing_name_, v, sizeof(self->thing_name_) - 1);
            self->thing_name_[sizeof(self->thing_name_) - 1] = '\0';
            nvs_helper::write_str("awscert", "thing", self->thing_name_);
            config_changed = true;
        } else if (klen == 7 && strncmp(k, "enabled", 7) == 0) {
            self->enabled_ = (v[0] == 't' || v[0] == '1');
            nvs_helper::write_bool("awscert", "enabled", self->enabled_);
            config_changed = true;
        } else if (klen == 4 && strncmp(k, "cert", 4) == 0 && vlen > 10) {
            // Зберігаємо PEM сертифікат у NVS як blob
            nvs_helper::write_blob("awscert", "cert", v, vlen);
            strncpy(s_device_cert_pem_, v, CERT_BUF_SIZE - 1);
            s_device_cert_pem_[CERT_BUF_SIZE - 1] = '\0';
            cert_uploaded = true;
            ESP_LOGI(TAG, "Device certificate saved (%d bytes)", vlen);
        } else if (klen == 3 && strncmp(k, "key", 3) == 0 && vlen > 10) {
            // Зберігаємо PEM private key у NVS як blob
            nvs_helper::write_blob("awscert", "key", v, vlen);
            strncpy(s_device_key_pem_, v, CERT_BUF_SIZE - 1);
            s_device_key_pem_[CERT_BUF_SIZE - 1] = '\0';
            cert_uploaded = true;
            ESP_LOGI(TAG, "Private key saved (%d bytes)", vlen);
        }

        body[tokens[i + 1].end] = saved;
    }

    if (cert_uploaded) {
        self->cert_loaded_ = (s_device_cert_pem_[0] != '\0' && s_device_key_pem_[0] != '\0');
    }

    if (config_changed) {
        ESP_LOGI(TAG, "Config saved: endpoint=%s, thing=%s, enabled=%d",
                 self->endpoint_, self->thing_name_, self->enabled_);
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"ok\":true}", 11);
}

void AwsIotService::register_http_handlers() {
    if (!server_) return;

    httpd_uri_t get_cloud = {};
    get_cloud.uri      = "/api/cloud";
    get_cloud.method   = HTTP_GET;
    get_cloud.handler  = handle_get_cloud;
    get_cloud.user_ctx = this;
    httpd_register_uri_handler(server_, &get_cloud);

    httpd_uri_t post_cloud = {};
    post_cloud.uri      = "/api/cloud";
    post_cloud.method   = HTTP_POST;
    post_cloud.handler  = handle_post_cloud;
    post_cloud.user_ctx = this;
    httpd_register_uri_handler(server_, &post_cloud);

    // OPTIONS для CORS preflight
    httpd_uri_t options = {};
    options.uri      = "/api/cloud";
    options.method   = HTTP_OPTIONS;
    options.handler  = [](httpd_req_t* req) -> esp_err_t {
        set_cors_headers(req);
        httpd_resp_send(req, nullptr, 0);
        return ESP_OK;
    };
    httpd_register_uri_handler(server_, &options);

    ESP_LOGI(TAG, "HTTP handlers registered (GET/POST /api/cloud)");
}

} // namespace modesp
