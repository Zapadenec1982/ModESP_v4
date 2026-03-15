/**
 * @file aws_iot_service.h
 * @brief AWS IoT Core cloud service — mTLS, Device Shadow, IoT Jobs
 *
 * Compile-time alternative to MqttService (Kconfig MODESP_CLOUD_AWS).
 * Uses esp-aws-iot SDK (coreMQTT, Shadow, Fleet Provisioning, OTA).
 *
 * Certificates stored in NVS namespace "awscert" (PEM blobs).
 * AWS Root CA (AmazonRootCA1) embedded in firmware.
 *
 * Default: disabled until endpoint + certificates configured via /api/cloud.
 */

#pragma once

#include "modesp/base_module.h"
#include "esp_http_server.h"

namespace modesp {

class SharedState;

class AwsIotService : public BaseModule {
public:
    AwsIotService() : BaseModule("cloud", ModulePriority::HIGH) {}

    bool on_init() override;
    void on_update(uint32_t dt_ms) override;
    void on_stop() override;

    // Dependency injection (сумісний інтерфейс з MqttService)
    void set_state(SharedState* s) { state_ = s; }
    void set_http_server(httpd_handle_t server);

    // Public API
    bool is_connected() const { return connected_; }
    bool is_enabled() const { return enabled_; }

private:
    SharedState* state_ = nullptr;
    httpd_handle_t server_ = nullptr;
    bool http_registered_ = false;

    // NVS config (namespace "awscert")
    char endpoint_[128] = {};     // xxxxx.iot.region.amazonaws.com
    char thing_name_[64] = {};    // AWS Thing name
    char device_id_[8] = {};      // MAC-based (e.g., "A4CF12")
    bool enabled_ = false;
    bool connected_ = false;
    bool cert_loaded_ = false;

    // Сертифікати (статичні буфери в .bss, не heap)
    static constexpr size_t CERT_BUF_SIZE = 2048;
    static char s_device_cert_pem_[CERT_BUF_SIZE];
    static char s_device_key_pem_[CERT_BUF_SIZE];

    // Timers
    uint32_t publish_timer_ = 0;
    static constexpr uint32_t PUBLISH_INTERVAL_MS = 1000;

    uint32_t heartbeat_timer_ms_ = 0;
    static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 30000;

    // Delta-publish cache (аналогічно MqttService)
    static constexpr size_t MAX_PUBLISH_KEYS = 56;
    char last_payloads_[MAX_PUBLISH_KEYS][16] = {};
    uint32_t last_version_ = 0;

    // Internal methods
    void load_config();
    bool load_certificates();
    void register_http_handlers();

    // HTTP handlers
    static esp_err_t handle_get_cloud(httpd_req_t* req);
    static esp_err_t handle_post_cloud(httpd_req_t* req);
    static void set_cors_headers(httpd_req_t* req);
};

} // namespace modesp
