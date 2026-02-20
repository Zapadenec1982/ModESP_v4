/**
 * @file equipment_module.cpp
 * @brief Equipment Manager — єдиний власник HAL drivers
 *
 * Потік даних кожен цикл:
 *   1. apply_outputs()     — застосовує relay з ПОПЕРЕДНЬОГО циклу
 *   2. read_sensors()      — читає сенсори → SharedState
 *   3. read_requests()     — читає req.* від бізнес-модулів
 *   4. apply_arbitration() — арбітраж + інтерлоки → нові outputs
 *   5. publish_state()     — публікує фактичний стан актуаторів
 */

#include "equipment_module.h"
#include "modesp/hal/driver_manager.h"
#include "esp_log.h"

static const char* TAG = "Equipment";

// ═══════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════

EquipmentModule::EquipmentModule()
    : BaseModule("equipment", modesp::ModulePriority::CRITICAL)
{}

// ═══════════════════════════════════════════════════════════════
// Driver binding — єдиний модуль з доступом до HAL
// ═══════════════════════════════════════════════════════════════

void EquipmentModule::bind_drivers(modesp::DriverManager& dm) {
    // Обов'язкові
    sensor_air_  = dm.find_sensor("air_temp");
    compressor_  = dm.find_actuator("compressor");

    // Опціональні
    sensor_evap_ = dm.find_sensor("evap_temp");
    heater_      = dm.find_actuator("heater");
    evap_fan_    = dm.find_actuator("evap_fan");
    cond_fan_    = dm.find_actuator("cond_fan");
    hg_valve_    = dm.find_actuator("hg_valve");

    if (!sensor_air_) {
        ESP_LOGE(TAG, "Sensor 'air_temp' not found — REQUIRED");
    }
    if (!compressor_) {
        ESP_LOGE(TAG, "Actuator 'compressor' not found — REQUIRED");
    }
    if (sensor_evap_) ESP_LOGI(TAG, "Evaporator sensor bound");
    if (heater_)      ESP_LOGI(TAG, "Heater bound");
    if (evap_fan_)    ESP_LOGI(TAG, "Evaporator fan bound");
    if (cond_fan_)    ESP_LOGI(TAG, "Condenser fan bound");
    if (hg_valve_)    ESP_LOGI(TAG, "Hot gas valve bound");
}

// ═══════════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════════

bool EquipmentModule::on_init() {
    if (!sensor_air_ || !compressor_) {
        ESP_LOGE(TAG, "Required drivers not bound — call bind_drivers() first");
        return false;
    }

    // Початковий стан в SharedState
    state_set("equipment.air_temp", 0.0f);
    state_set("equipment.evap_temp", 0.0f);
    state_set("equipment.sensor1_ok", false);
    state_set("equipment.sensor2_ok", false);
    state_set("equipment.compressor", false);
    state_set("equipment.heater", false);
    state_set("equipment.evap_fan", false);
    state_set("equipment.cond_fan", false);
    state_set("equipment.hg_valve", false);
    state_set("equipment.door_open", false);

    ESP_LOGI(TAG, "Initialized (air_sensor=%s, compressor=%s)",
             sensor_air_ ? "OK" : "MISSING",
             compressor_ ? "OK" : "MISSING");
    return true;
}

void EquipmentModule::on_update(uint32_t dt_ms) {
    // 1. Застосовуємо relay стани з попереднього циклу
    apply_outputs();

    // 2. Читаємо сенсори → SharedState
    read_sensors();

    // 3. Читаємо requests від бізнес-модулів
    read_requests();

    // 4. Арбітраж + інтерлоки → визначаємо нові outputs
    apply_arbitration();

    // 5. Публікуємо фактичний стан актуаторів
    publish_state();
}

void EquipmentModule::on_message(const etl::imessage& msg) {
    // Safe mode — все вимкнути (NEW-001 fix: було 1, а SYSTEM_SAFE_MODE = 7)
    if (msg.get_message_id() == modesp::msg_id::SYSTEM_SAFE_MODE) {
        out_ = {};
        apply_outputs();
        publish_state();
        ESP_LOGW(TAG, "SAFE MODE — all outputs OFF");
    }
}

void EquipmentModule::on_stop() {
    // Аварійна зупинка — все вимкнути
    out_ = {};
    apply_outputs();
    ESP_LOGI(TAG, "Equipment stopped — all outputs OFF");
}

// ═══════════════════════════════════════════════════════════════
// Читання сенсорів
// ═══════════════════════════════════════════════════════════════

void EquipmentModule::read_sensors() {
    // Датчик камери (обов'язковий)
    if (sensor_air_) {
        float temp = 0.0f;
        if (sensor_air_->read(temp)) {
            air_temp_ = temp;
            has_air_temp_ = true;
            state_set("equipment.air_temp", air_temp_);
            state_set("equipment.sensor1_ok", true);
        } else {
            // Датчик не відповідає
            if (has_air_temp_) {
                // Зберігаємо останнє значення, але помічаємо проблему
                state_set("equipment.sensor1_ok", false);
            }
        }
    }

    // Датчик випарника (опціональний)
    if (sensor_evap_) {
        float temp = 0.0f;
        if (sensor_evap_->read(temp)) {
            evap_temp_ = temp;
            has_evap_temp_ = true;
            state_set("equipment.evap_temp", evap_temp_);
            state_set("equipment.sensor2_ok", true);
        } else {
            state_set("equipment.sensor2_ok", false);
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// Читання requests з SharedState
// ═══════════════════════════════════════════════════════════════

void EquipmentModule::read_requests() {
    auto get_bool = [this](const char* key) -> bool {
        auto v = state_get(key);
        if (!v.has_value()) return false;
        const auto* bp = etl::get_if<bool>(&v.value());
        return bp ? *bp : false;
    };

    // Thermostat requests
    req_.therm_compressor = get_bool("thermostat.req.compressor");
    req_.therm_evap_fan   = get_bool("thermostat.req.evap_fan");
    req_.therm_cond_fan   = get_bool("thermostat.req.cond_fan");

    // Defrost requests
    req_.defrost_active   = get_bool("defrost.active");
    req_.def_compressor   = get_bool("defrost.req.compressor");
    req_.def_heater       = get_bool("defrost.req.heater");
    req_.def_evap_fan     = get_bool("defrost.req.evap_fan");
    req_.def_cond_fan     = get_bool("defrost.req.cond_fan");
    req_.def_hg_valve     = get_bool("defrost.req.hg_valve");

    // Protection
    req_.protection_lockout = get_bool("protection.lockout");
}

// ═══════════════════════════════════════════════════════════════
// Арбітраж + інтерлоки
// ═══════════════════════════════════════════════════════════════

void EquipmentModule::apply_arbitration() {
    // Protection lockout = все вимкнено (найвищий пріоритет)
    if (req_.protection_lockout) {
        out_ = {};
        return;
    }

    // Defrost active = defrost requests мають пріоритет
    if (req_.defrost_active) {
        out_.compressor = req_.def_compressor;
        out_.heater     = req_.def_heater;
        out_.evap_fan   = req_.def_evap_fan;
        out_.cond_fan   = req_.def_cond_fan;
        out_.hg_valve   = req_.def_hg_valve;
    } else {
        // Нормальний режим: thermostat requests
        out_.compressor = req_.therm_compressor;
        out_.heater     = false;   // Тільки defrost може ввімкнути
        out_.evap_fan   = req_.therm_evap_fan;
        out_.cond_fan   = req_.therm_cond_fan;
        out_.hg_valve   = false;   // Тільки defrost може ввімкнути
    }

    // === ІНТЕРЛОКИ (hardcoded, неможливо обійти) ===

    // 1. Тен і компресор НІКОЛИ одночасно
    if (out_.heater && out_.compressor) {
        out_.compressor = false;
        ESP_LOGW(TAG, "INTERLOCK: heater+compressor → compressor OFF");
    }

    // 2. Тен і клапан ГГ НІКОЛИ одночасно
    if (out_.heater && out_.hg_valve) {
        out_.hg_valve = false;
        ESP_LOGW(TAG, "INTERLOCK: heater+hg_valve → hg_valve OFF");
    }
}

// ═══════════════════════════════════════════════════════════════
// Застосування outputs до relay
// ═══════════════════════════════════════════════════════════════

void EquipmentModule::apply_outputs() {
    if (compressor_) compressor_->set(out_.compressor);
    if (heater_)     heater_->set(out_.heater);
    if (evap_fan_)   evap_fan_->set(out_.evap_fan);
    if (cond_fan_)   cond_fan_->set(out_.cond_fan);
    if (hg_valve_)   hg_valve_->set(out_.hg_valve);
}

// ═══════════════════════════════════════════════════════════════
// Публікація фактичного стану актуаторів
// ═══════════════════════════════════════════════════════════════

void EquipmentModule::publish_state() {
    state_set("equipment.compressor", out_.compressor);
    state_set("equipment.heater", out_.heater);
    state_set("equipment.evap_fan", out_.evap_fan);
    state_set("equipment.cond_fan", out_.cond_fan);
    state_set("equipment.hg_valve", out_.hg_valve);
}
