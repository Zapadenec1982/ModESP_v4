/**
 * @file shared_state_host.cpp
 * @brief HOST BUILD: shared_state.cpp adapted for Windows host compilation.
 */

// ── HOST BUILD: intercept ESP-IDF headers ──
#include "mocks/freertos_mock.h"
#include "mocks/esp_log_mock.h"
#include "mocks/esp_timer_mock.h"

// ── Real ModESP headers (compile on host now that types are defined) ──
#include "modesp/shared_state.h"

// Implementation — identical to components/modesp_core/src/shared_state.cpp

static const char* TAG = "SharedState";

namespace modesp {

SharedState::SharedState() {
    mutex_ = xSemaphoreCreateMutex();
    configASSERT(mutex_ != nullptr);
}

SharedState::~SharedState() {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}

bool SharedState::set(const StateKey& key, const StateValue& value) {
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
        set_failures_++;
        ESP_LOGE(TAG, "Mutex timeout on set('%s') [failures=%lu]",
                 key.c_str(), (unsigned long)set_failures_);
        return false;
    }

    if (map_.full() && !map_.count(key)) {
        set_failures_++;
        ESP_LOGE(TAG, "STATE MAP FULL (%d/%d) — cannot add '%s' [failures=%lu]",
                 (int)map_.size(), MODESP_MAX_STATE_ENTRIES,
                 key.c_str(), (unsigned long)set_failures_);
        xSemaphoreGive(mutex_);
        return false;
    }

    bool changed = true;
    auto it = map_.find(key);
    if (it != map_.end()) {
        changed = !(it->second == value);
    }

    map_[key] = value;
    if (changed) {
        version_++;
    }

    auto cb = persist_cb_;
    auto ud = persist_user_data_;
    xSemaphoreGive(mutex_);

    if (changed && cb) {
        cb(key, value, ud);
    }

    return true;
}

etl::optional<StateValue> SharedState::get(const StateKey& key) const {
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
        return etl::nullopt;
    }

    auto it = map_.find(key);
    etl::optional<StateValue> result;
    if (it != map_.end()) {
        result = it->second;
    }
    xSemaphoreGive(mutex_);
    return result;
}

bool SharedState::has(const StateKey& key) const {
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) return false;
    bool result = map_.count(key) > 0;
    xSemaphoreGive(mutex_);
    return result;
}

bool SharedState::remove(const StateKey& key) {
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) return false;
    size_t removed = map_.erase(key);
    xSemaphoreGive(mutex_);
    return removed > 0;
}

size_t SharedState::size() const {
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) return 0;
    size_t s = map_.size();
    xSemaphoreGive(mutex_);
    return s;
}

void SharedState::clear() {
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) return;
    map_.clear();
    xSemaphoreGive(mutex_);
}

bool SharedState::set(const char* key, int32_t value) {
    return set(StateKey(key), StateValue(value));
}

bool SharedState::set(const char* key, float value) {
    return set(StateKey(key), StateValue(value));
}

bool SharedState::set(const char* key, bool value) {
    return set(StateKey(key), StateValue(value));
}

bool SharedState::set(const char* key, const char* value) {
    return set(StateKey(key), StateValue(StringValue(value)));
}

etl::optional<StateValue> SharedState::get(const char* key) const {
    return get(StateKey(key));
}

uint32_t SharedState::version() const {
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) return 0;
    uint32_t v = version_;
    xSemaphoreGive(mutex_);
    return v;
}

void SharedState::set_persist_callback(PersistCallback cb, void* user_data) {
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) return;
    persist_cb_ = cb;
    persist_user_data_ = user_data;
    xSemaphoreGive(mutex_);
}

void SharedState::for_each(IterCallback cb, void* user_data) const {
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) return;
    for (auto& pair : map_) {
        cb(pair.first, pair.second, user_data);
    }
    xSemaphoreGive(mutex_);
}

} // namespace modesp
