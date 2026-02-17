/**
 * @file persist_service.cpp
 * @brief Auto-persist readwrite settings to NVS
 *
 * При boot: ітерує STATE_META, для persist==true читає NVS → SharedState.
 * При runtime: SharedState callback ставить dirty flag, on_update() пише з debounce.
 */

#include "modesp/services/persist_service.h"
#include "modesp/services/nvs_helper.h"
#include "state_meta.h"
#include "esp_log.h"

#include <cstdio>
#include <cstring>

static const char* TAG = "Persist";

namespace modesp {

PersistService::PersistService()
    : BaseModule("persist", ModulePriority::CRITICAL)
{
    memset(dirty_, 0, sizeof(dirty_));
}

void PersistService::make_nvs_key(size_t index, char* out, size_t out_size) {
    snprintf(out, out_size, "p%u", (unsigned)index);
}

bool PersistService::on_init() {
    if (!ext_state_) {
        ESP_LOGE(TAG, "SharedState не встановлений — викличте set_state() перед init");
        return false;
    }

    // Відновлюємо збережені значення з NVS
    restore_from_nvs();

    // Реєструємо persist callback
    ext_state_->set_persist_callback(on_state_changed, this);

    ESP_LOGI(TAG, "Initialized (debounce=%lu ms)", DEBOUNCE_MS);
    return true;
}

void PersistService::restore_from_nvs() {
    if (!ext_state_) return;

    int restored = 0;

    for (size_t i = 0; i < gen::STATE_META_COUNT; i++) {
        const auto& meta = gen::STATE_META[i];
        if (!meta.persist) continue;

        char nvs_key[8];
        make_nvs_key(i, nvs_key, sizeof(nvs_key));

        if (strcmp(meta.type, "float") == 0) {
            float val = 0.0f;
            if (nvs_helper::read_float(NVS_NAMESPACE, nvs_key, val)) {
                ext_state_->set(meta.key, val);
                ESP_LOGI(TAG, "Restored %s = %.2f (NVS key: %s)",
                         meta.key, static_cast<double>(val), nvs_key);
                restored++;
            } else {
                // Немає в NVS — використовуємо default
                ext_state_->set(meta.key, meta.default_val);
                ESP_LOGI(TAG, "Default %s = %.2f (not in NVS)",
                         meta.key, static_cast<double>(meta.default_val));
            }
        } else if (strcmp(meta.type, "int") == 0) {
            int32_t val = 0;
            if (nvs_helper::read_i32(NVS_NAMESPACE, nvs_key, val)) {
                ext_state_->set(meta.key, val);
                ESP_LOGI(TAG, "Restored %s = %ld (NVS key: %s)",
                         meta.key, (long)val, nvs_key);
                restored++;
            } else {
                ext_state_->set(meta.key, static_cast<int32_t>(meta.default_val));
            }
        } else if (strcmp(meta.type, "bool") == 0) {
            bool val = false;
            if (nvs_helper::read_bool(NVS_NAMESPACE, nvs_key, val)) {
                ext_state_->set(meta.key, val);
                ESP_LOGI(TAG, "Restored %s = %s (NVS key: %s)",
                         meta.key, val ? "true" : "false", nvs_key);
                restored++;
            } else {
                ext_state_->set(meta.key, meta.default_val != 0.0f);
            }
        }
    }

    ESP_LOGI(TAG, "Restored %d keys from NVS", restored);
}

void PersistService::on_state_changed(const StateKey& key, const StateValue& value, void* user_data) {
    auto* self = static_cast<PersistService*>(user_data);
    (void)value;

    // Шукаємо ключ в STATE_META з persist==true
    for (size_t i = 0; i < gen::STATE_META_COUNT && i < MAX_PERSIST_KEYS; i++) {
        const auto& meta = gen::STATE_META[i];
        if (!meta.persist) continue;

        // Порівнюємо ключі
        if (key == StateKey(meta.key)) {
            self->dirty_[i] = true;
            self->debounce_timer_ = 0;  // Скидаємо таймер
            return;
        }
    }
}

void PersistService::on_update(uint32_t dt_ms) {
    // Перевіряємо чи є dirty keys
    bool has_dirty = false;
    for (size_t i = 0; i < gen::STATE_META_COUNT && i < MAX_PERSIST_KEYS; i++) {
        if (dirty_[i]) {
            has_dirty = true;
            break;
        }
    }

    if (!has_dirty) return;

    // Debounce — чекаємо DEBOUNCE_MS після останньої зміни
    debounce_timer_ += dt_ms;
    if (debounce_timer_ < DEBOUNCE_MS) return;

    flush_to_nvs();
}

void PersistService::flush_to_nvs() {
    if (!ext_state_) return;

    int saved = 0;

    for (size_t i = 0; i < gen::STATE_META_COUNT && i < MAX_PERSIST_KEYS; i++) {
        if (!dirty_[i]) continue;
        dirty_[i] = false;

        const auto& meta = gen::STATE_META[i];
        if (!meta.persist) continue;

        auto val = ext_state_->get(meta.key);
        if (!val.has_value()) continue;

        char nvs_key[8];
        make_nvs_key(i, nvs_key, sizeof(nvs_key));

        bool ok = false;
        if (strcmp(meta.type, "float") == 0) {
            const auto* fp = etl::get_if<float>(&val.value());
            if (fp) ok = nvs_helper::write_float(NVS_NAMESPACE, nvs_key, *fp);
        } else if (strcmp(meta.type, "int") == 0) {
            const auto* ip = etl::get_if<int32_t>(&val.value());
            if (ip) ok = nvs_helper::write_i32(NVS_NAMESPACE, nvs_key, *ip);
        } else if (strcmp(meta.type, "bool") == 0) {
            const auto* bp = etl::get_if<bool>(&val.value());
            if (bp) ok = nvs_helper::write_bool(NVS_NAMESPACE, nvs_key, *bp);
        }

        if (ok) {
            saved++;
            ESP_LOGI(TAG, "Saved %s to NVS (key: %s)", meta.key, nvs_key);
        } else {
            ESP_LOGW(TAG, "Failed to save %s to NVS", meta.key);
        }
    }

    if (saved > 0) {
        ESP_LOGI(TAG, "Flushed %d keys to NVS", saved);
    }

    debounce_timer_ = 0;
}

} // namespace modesp
