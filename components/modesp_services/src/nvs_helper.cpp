/**
 * @file nvs_helper.cpp
 * @brief NVS read/write helper implementations
 */

#include "modesp/services/nvs_helper.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#include <cstring>

static const char* TAG = "NVS";

namespace modesp {
namespace nvs_helper {

bool init() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "NVS initialized");
    return true;
}

bool read_str(const char* ns, const char* key, char* out, size_t max_len) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
    if (err != ESP_OK) return false;

    size_t len = max_len;
    err = nvs_get_str(handle, key, out, &len);
    nvs_close(handle);
    return err == ESP_OK;
}

bool write_str(const char* ns, const char* key, const char* value) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open '%s' failed: %s", ns, esp_err_to_name(err));
        return false;
    }

    err = nvs_set_str(handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS write '%s.%s' failed: %s", ns, key, esp_err_to_name(err));
    }
    return err == ESP_OK;
}

bool read_float(const char* ns, const char* key, float& out) {
    // NVS has no native float — store as blob of 4 bytes
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
    if (err != ESP_OK) return false;

    size_t len = sizeof(float);
    err = nvs_get_blob(handle, key, &out, &len);
    nvs_close(handle);
    return err == ESP_OK && len == sizeof(float);
}

bool write_float(const char* ns, const char* key, float value) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;

    err = nvs_set_blob(handle, key, &value, sizeof(float));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err == ESP_OK;
}

bool read_i32(const char* ns, const char* key, int32_t& out) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
    if (err != ESP_OK) return false;

    err = nvs_get_i32(handle, key, &out);
    nvs_close(handle);
    return err == ESP_OK;
}

bool write_i32(const char* ns, const char* key, int32_t value) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;

    err = nvs_set_i32(handle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err == ESP_OK;
}

bool read_bool(const char* ns, const char* key, bool& out) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
    if (err != ESP_OK) return false;

    uint8_t val = 0;
    err = nvs_get_u8(handle, key, &val);
    nvs_close(handle);
    if (err == ESP_OK) {
        out = (val != 0);
    }
    return err == ESP_OK;
}

bool write_bool(const char* ns, const char* key, bool value) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) return false;

    err = nvs_set_u8(handle, key, value ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err == ESP_OK;
}

} // namespace nvs_helper
} // namespace modesp
