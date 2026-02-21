/**
 * @file shared_state.h
 * @brief Thread-safe key-value сховище стану
 *
 * Централізоване сховище для обміну даними між модулями.
 * Використовує etl::unordered_map з фіксованим розміром.
 *
 * Характеристики:
 *   - Zero heap allocation
 *   - Thread-safe (FreeRTOS mutex)
 *   - O(1) average доступ
 *   - ~6KB RAM при 96 entries
 */

#pragma once

#include "modesp/types.h"
#include "etl/unordered_map.h"
#include "etl/optional.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace modesp {

#ifndef MODESP_MAX_STATE_ENTRIES
#define MODESP_MAX_STATE_ENTRIES 128
#endif

class SharedState {
public:
    SharedState();
    ~SharedState();

    // Не копіюється
    SharedState(const SharedState&) = delete;
    SharedState& operator=(const SharedState&) = delete;

    // ── Основний API ──
    bool set(const StateKey& key, const StateValue& value);
    etl::optional<StateValue> get(const StateKey& key) const;
    bool has(const StateKey& key) const;
    bool remove(const StateKey& key);
    size_t size() const;
    void clear();

    // ── Зручні обгортки ──
    bool set(const char* key, int32_t value);
    bool set(const char* key, float value);
    bool set(const char* key, bool value);
    bool set(const char* key, const char* value);

    etl::optional<StateValue> get(const char* key) const;

    // ── Ітерація (для серіалізації, з блокуванням!) ──
    using Map = etl::unordered_map<StateKey, StateValue, MODESP_MAX_STATE_ENTRIES>;

    // Callback викликається під mutex — не робити довгих операцій!
    using IterCallback = void(*)(const StateKey& key, const StateValue& value, void* user_data);
    void for_each(IterCallback cb, void* user_data) const;

    // Version counter — incremented on every successful set().
    // Used by WsService to detect state changes without full comparison.
    uint32_t version() const;

    // BUG-018: лічильник відмов set() для діагностики
    uint32_t set_failures() const { return set_failures_; }

    // ── Persist callback ──
    // Викликається ПІСЛЯ set() якщо значення змінилось.
    // PersistService реєструє callback для збереження в NVS.
    using PersistCallback = void(*)(const StateKey& key, const StateValue& value, void* user_data);
    void set_persist_callback(PersistCallback cb, void* user_data = nullptr);

private:
    Map map_;
    mutable SemaphoreHandle_t mutex_;
    uint32_t version_ = 0;
    uint32_t set_failures_ = 0;  // BUG-018
    PersistCallback persist_cb_ = nullptr;
    void* persist_user_data_ = nullptr;
};

} // namespace modesp
