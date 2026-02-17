/**
 * @file wifi_service.cpp
 * @brief WiFi STA/AP management implementation
 */

#include "modesp/net/wifi_service.h"
#include "modesp/services/nvs_helper.h"
#include "esp_mac.h"
#include "esp_log.h"

#include <cstring>
#include <cstdio>

static const char* TAG = "WiFi";

namespace modesp {

// ── Event handler (static, dispatches to instance) ──────────────

void WiFiService::event_handler(void* arg, esp_event_base_t base,
                                 int32_t event_id, void* event_data) {
    auto* self = static_cast<WiFiService*>(arg);
    if (base == WIFI_EVENT) {
        self->handle_wifi_event(event_id, event_data);
    } else if (base == IP_EVENT) {
        self->handle_ip_event(event_id, event_data);
    }
}

void WiFiService::handle_wifi_event(int32_t event_id, void* event_data) {
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "STA started, connecting...");
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            connected_ = false;
            ip_str_[0] = '\0';
            state_set("wifi.connected", false);
            state_set("wifi.ip", "");
            if (!ap_mode_) {
                reconnect_pending_ = true;
                ESP_LOGW(TAG, "Disconnected, will retry (attempt %d)", retry_count_ + 1);
            }
            break;
        }

        case WIFI_EVENT_AP_STACONNECTED: {
            auto* event = static_cast<wifi_event_ap_staconnected_t*>(event_data);
            ESP_LOGI(TAG, "AP: station connected (AID=%d)", event->aid);
            break;
        }

        case WIFI_EVENT_AP_STADISCONNECTED: {
            auto* event = static_cast<wifi_event_ap_stadisconnected_t*>(event_data);
            ESP_LOGI(TAG, "AP: station disconnected (AID=%d)", event->aid);
            break;
        }

        default:
            break;
    }
}

void WiFiService::handle_ip_event(int32_t event_id, void* event_data) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
        auto* event = static_cast<ip_event_got_ip_t*>(event_data);
        snprintf(ip_str_, sizeof(ip_str_), IPSTR, IP2STR(&event->ip_info.ip));

        connected_ = true;
        reconnect_pending_ = false;
        retry_count_ = 0;
        reconnect_interval_ = 2000;

        state_set("wifi.connected", true);
        state_set("wifi.ssid", ssid_);
        state_set("wifi.ip", ip_str_);

        ESP_LOGI(TAG, "Connected! IP: %s", ip_str_);
    }
}

// ── Lifecycle ───────────────────────────────────────────────────

bool WiFiService::on_init() {
    ESP_LOGI(TAG, "Initializing WiFi...");

    // Initialize TCP/IP and event loop (safe to call multiple times)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, this, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, this, nullptr));

    // Try to load credentials from NVS
    if (load_credentials()) {
        ESP_LOGI(TAG, "Credentials found, starting STA mode (SSID: %s)", ssid_);
        state_set("wifi.ssid", ssid_);
        return start_sta();
    } else {
        ESP_LOGW(TAG, "No credentials, starting AP mode");
        return start_ap();
    }
}

void WiFiService::on_update(uint32_t dt_ms) {
    // Reconnect logic (STA mode only)
    if (reconnect_pending_ && !ap_mode_) {
        reconnect_timer_ += dt_ms;
        if (reconnect_timer_ >= reconnect_interval_) {
            reconnect_timer_ = 0;

            if (retry_count_ >= MAX_RETRIES) {
                ESP_LOGW(TAG, "Max retries reached, switching to AP mode");
                reconnect_pending_ = false;
                esp_wifi_stop();
                start_ap();
                return;
            }

            retry_count_++;
            ESP_LOGI(TAG, "Reconnect attempt %d/%d (backoff %lu ms)",
                     retry_count_, MAX_RETRIES, (unsigned long)reconnect_interval_);
            esp_wifi_connect();

            // Exponential backoff
            reconnect_interval_ *= 2;
            if (reconnect_interval_ > MAX_BACKOFF_MS) {
                reconnect_interval_ = MAX_BACKOFF_MS;
            }
        }
    }

    // RSSI update (STA mode, connected)
    if (connected_ && !ap_mode_) {
        rssi_timer_ += dt_ms;
        if (rssi_timer_ >= RSSI_INTERVAL_MS) {
            rssi_timer_ = 0;
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                state_set("wifi.rssi", static_cast<int32_t>(ap_info.rssi));
            }
        }
    }
}

// ── Credentials ─────────────────────────────────────────────────

bool WiFiService::load_credentials() {
    bool ok = nvs_helper::read_str("wifi", "ssid", ssid_, sizeof(ssid_));
    if (!ok || ssid_[0] == '\0') return false;

    nvs_helper::read_str("wifi", "pass", password_, sizeof(password_));
    return true;
}

bool WiFiService::save_credentials(const char* ssid, const char* password) {
    if (!ssid || ssid[0] == '\0') return false;

    bool ok = nvs_helper::write_str("wifi", "ssid", ssid);
    if (ok && password) {
        ok = nvs_helper::write_str("wifi", "pass", password);
    }
    if (ok) {
        strncpy(ssid_, ssid, sizeof(ssid_) - 1);
        ssid_[sizeof(ssid_) - 1] = '\0';
        if (password) {
            strncpy(password_, password, sizeof(password_) - 1);
            password_[sizeof(password_) - 1] = '\0';
        }
        ESP_LOGI(TAG, "Credentials saved for SSID: %s", ssid_);
    }
    return ok;
}

void WiFiService::request_reconnect() {
    if (ap_mode_) {
        // Switch back to STA if credentials exist
        if (ssid_[0] != '\0') {
            esp_wifi_stop();
            ap_mode_ = false;
            start_sta();
        }
    } else {
        esp_wifi_disconnect();
        retry_count_ = 0;
        reconnect_interval_ = 2000;
        reconnect_pending_ = true;
        reconnect_timer_ = reconnect_interval_; // trigger immediate reconnect
    }
}

// ── WiFi Scan ───────────────────────────────────────────────────

bool WiFiService::start_scan() {
    scan_done_ = false;

    // Scanning requires STA (or APSTA) mode.
    // If we're in pure AP mode, temporarily switch to APSTA.
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_wifi_get_mode(&mode);

    bool switched_to_apsta = false;
    if (mode == WIFI_MODE_AP) {
        ESP_LOGI(TAG, "Switching to APSTA mode for scan...");
        esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to switch to APSTA: %s", esp_err_to_name(err));
            return false;
        }
        switched_to_apsta = true;
    } else if (mode == WIFI_MODE_NULL) {
        ESP_LOGW(TAG, "Cannot scan: WiFi not started");
        return false;
    }

    wifi_scan_config_t scan_config = {};
    scan_config.show_hidden = false;
    scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scan_config.scan_time.active.min = 100;
    scan_config.scan_time.active.max = 300;

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);  // blocking scan

    // Restore AP-only mode if we switched
    if (switched_to_apsta) {
        esp_wifi_set_mode(WIFI_MODE_AP);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Scan start failed: %s", esp_err_to_name(err));
        return false;
    }

    scan_done_ = true;
    ESP_LOGI(TAG, "WiFi scan completed");
    return true;
}

int WiFiService::get_scan_results(ScanResult* out, size_t max_results) {
    if (!scan_done_) return 0;

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) return 0;

    if (ap_count > MAX_SCAN_RESULTS) ap_count = MAX_SCAN_RESULTS;
    if (ap_count > max_results) ap_count = static_cast<uint16_t>(max_results);

    wifi_ap_record_t* ap_records = new (std::nothrow) wifi_ap_record_t[ap_count];
    if (!ap_records) return 0;

    esp_wifi_scan_get_ap_records(&ap_count, ap_records);

    for (uint16_t i = 0; i < ap_count; i++) {
        strncpy(out[i].ssid, reinterpret_cast<const char*>(ap_records[i].ssid), 32);
        out[i].ssid[32] = '\0';
        out[i].rssi = ap_records[i].rssi;
        out[i].authmode = static_cast<uint8_t>(ap_records[i].authmode);
    }

    delete[] ap_records;
    scan_done_ = false;
    return static_cast<int>(ap_count);
}

// ── STA / AP ────────────────────────────────────────────────────

bool WiFiService::start_sta() {
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config = {};
    strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid),
            ssid_, sizeof(wifi_config.sta.ssid) - 1);
    strncpy(reinterpret_cast<char*>(wifi_config.sta.password),
            password_, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ap_mode_ = false;
    state_set("wifi.mode", "sta");
    return true;
}

bool WiFiService::start_ap() {
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    // Generate SSID from MAC: ModESP-XXXX
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    char ap_ssid[32];
    snprintf(ap_ssid, sizeof(ap_ssid), "ModESP-%02X%02X", mac[4], mac[5]);

    wifi_config_t wifi_config = {};
    strncpy(reinterpret_cast<char*>(wifi_config.ap.ssid),
            ap_ssid, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = static_cast<uint8_t>(strlen(ap_ssid));
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 2;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ap_mode_ = true;
    connected_ = false;

    // AP mode IP is always 192.168.4.1
    strncpy(ip_str_, "192.168.4.1", sizeof(ip_str_));
    state_set("wifi.mode", "ap");
    state_set("wifi.ssid", ap_ssid);
    state_set("wifi.ip", ip_str_);
    state_set("wifi.connected", false);

    ESP_LOGI(TAG, "AP mode started: %s (open, no password)", ap_ssid);
    return true;
}

} // namespace modesp
