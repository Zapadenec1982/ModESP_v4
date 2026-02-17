/**
 * @file wifi_service.h
 * @brief WiFi station/AP management service
 *
 * Connects to a configured AP (STA mode) or creates a fallback AP
 * if no credentials are stored in NVS. Handles reconnection with
 * exponential backoff.
 */

#pragma once

#include "modesp/base_module.h"
#include "esp_wifi.h"
#include "esp_event.h"

namespace modesp {

class WiFiService : public BaseModule {
public:
    WiFiService() : BaseModule("wifi", ModulePriority::HIGH) {}

    bool on_init() override;
    void on_update(uint32_t dt_ms) override;

    // Current state
    bool is_connected() const { return connected_; }
    const char* ip_address() const { return ip_str_; }
    bool is_ap_mode() const { return ap_mode_; }

    // Control
    bool save_credentials(const char* ssid, const char* password);
    void request_reconnect();

    // Scan
    static constexpr size_t MAX_SCAN_RESULTS = 20;
    struct ScanResult {
        char ssid[33];
        int8_t rssi;
        uint8_t authmode;  // wifi_auth_mode_t
    };
    /// Start async scan. Returns true if scan was initiated.
    bool start_scan();
    /// Get scan results. Returns number of entries filled (0 if scan not done).
    int  get_scan_results(ScanResult* out, size_t max_results);
    bool is_scan_done() const { return scan_done_; }

private:
    bool connected_ = false;
    bool ap_mode_ = false;
    char ip_str_[16] = {};
    char ssid_[33] = {};
    char password_[65] = {};

    // Scan state
    bool scan_done_ = false;

    // Reconnect logic
    uint32_t reconnect_timer_ = 0;
    uint32_t reconnect_interval_ = 2000;
    uint8_t  retry_count_ = 0;
    bool     reconnect_pending_ = false;
    static constexpr uint8_t MAX_RETRIES = 10;
    static constexpr uint32_t MAX_BACKOFF_MS = 32000;

    // RSSI update throttle
    uint32_t rssi_timer_ = 0;
    static constexpr uint32_t RSSI_INTERVAL_MS = 10000;

    bool load_credentials();
    bool start_sta();
    bool start_ap();

    // ESP event handler (static because ESP-IDF API requires function pointer)
    static void event_handler(void* arg, esp_event_base_t base,
                              int32_t event_id, void* event_data);
    void handle_wifi_event(int32_t event_id, void* event_data);
    void handle_ip_event(int32_t event_id, void* event_data);
};

} // namespace modesp
