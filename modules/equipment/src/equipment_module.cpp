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

    // Опціональні сенсори
    sensor_evap_ = dm.find_sensor("evap_temp");
    sensor_cond_ = dm.find_sensor("condenser_temp");

    // Актуатори
    heater_      = dm.find_actuator("heater");
    evap_fan_    = dm.find_actuator("evap_fan");
    cond_fan_    = dm.find_actuator("cond_fan");
    hg_valve_    = dm.find_actuator("hg_valve");

    // Дискретні входи
    door_sensor_  = dm.find_sensor("door_contact");
    night_sensor_ = dm.find_sensor("night_input");

    if (!sensor_air_) {
        ESP_LOGE(TAG, "Sensor 'air_temp' not found — REQUIRED");
    }
    if (!compressor_) {
        ESP_LOGE(TAG, "Actuator 'compressor' not found — REQUIRED");
    }
    if (sensor_evap_) ESP_LOGI(TAG, "Evaporator sensor bound");
    if (sensor_cond_) ESP_LOGI(TAG, "Condenser sensor bound");
    if (heater_)      ESP_LOGI(TAG, "Heater bound");
    if (evap_fan_)    ESP_LOGI(TAG, "Evaporator fan bound");
    if (cond_fan_)    ESP_LOGI(TAG, "Condenser fan bound");
    if (hg_valve_)    ESP_LOGI(TAG, "Hot gas valve bound");
    if (door_sensor_) ESP_LOGI(TAG, "Door contact bound");
    if (night_sensor_) ESP_LOGI(TAG, "Night input bound");
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
    // Оптимістична ініціалізація: sensor_ok = true щоб Protection
    // не спрацьовувала ERR1/ERR2 до першого реального read().
    // DS18B20 потребує ~750ms на першу конверсію — перші read() фейляться.
    // Якщо датчик не сконфігурований — теж true (не помилка).
    state_set("equipment.air_temp", 0.0f);
    state_set("equipment.evap_temp", 0.0f);
    state_set("equipment.cond_temp", 0.0f);
    state_set("equipment.sensor1_ok", true);
    state_set("equipment.sensor2_ok", true);
    state_set("equipment.compressor", false);
    state_set("equipment.heater", false);
    state_set("equipment.evap_fan", false);
    state_set("equipment.cond_fan", false);
    state_set("equipment.hg_valve", false);
    state_set("equipment.door_open", false);
    state_set("equipment.night_input", false);

    ESP_LOGI(TAG, "Initialized (air_sensor=%s, compressor=%s)",
             sensor_air_ ? "OK" : "MISSING",
             compressor_ ? "OK" : "MISSING");
    return true;
}

void EquipmentModule::on_update(uint32_t dt_ms) {
    // AUDIT-003: оновлюємо таймер компресора
    comp_since_ms_ += dt_ms;
    if (comp_since_ms_ > 999999) comp_since_ms_ = 999999;  // Запобігаємо overflow

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
            state_set("equipment.air_temp", air_temp_);
        }
        // is_healthy() враховує consecutive_errors (драйвер відстежує).
        // read() повертає true з кешованим значенням навіть коли датчик офлайн,
        // тому для статусу використовуємо is_healthy().
        state_set("equipment.sensor1_ok", sensor_air_->is_healthy());
    }

    // Датчик випарника (опціональний)
    if (sensor_evap_) {
        float temp = 0.0f;
        if (sensor_evap_->read(temp)) {
            evap_temp_ = temp;
            state_set("equipment.evap_temp", evap_temp_);
        }
        state_set("equipment.sensor2_ok", sensor_evap_->is_healthy());
    }

    // Датчик конденсатора (опціональний — DS18B20 або NTC)
    if (sensor_cond_) {
        float temp = 0.0f;
        if (sensor_cond_->read(temp)) {
            cond_temp_ = temp;
            state_set("equipment.cond_temp", cond_temp_);
        }
    }

    // Контакт дверей (опціональний — digital_input)
    if (door_sensor_) {
        float val = 0.0f;
        door_sensor_->read(val);
        state_set("equipment.door_open", val > 0.5f);
    }

    // Дискретний вхід нічного режиму (опціональний)
    if (night_sensor_) {
        float val = 0.0f;
        night_sensor_->read(val);
        state_set("equipment.night_input", val > 0.5f);
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

    // === AUDIT-003: Compressor anti-short-cycle (output-level) ===
    // Захищає компресор незалежно від джерела запиту (thermostat/defrost).
    // Працює на фактичному стані реле, а не на запитах бізнес-модулів.
    // ВАЖЛИВО: виконується ДО інтерлоків, щоб інтерлоки мали фінальне слово.
    if (out_.compressor != comp_actual_) {
        if (out_.compressor) {
            // Запит на ввімкнення — перевіряємо min OFF time
            if (comp_since_ms_ < COMP_MIN_OFF_MS) {
                out_.compressor = false;
                ESP_LOGD(TAG, "Compressor ON blocked — min OFF (%lu/%lu ms)",
                         comp_since_ms_, COMP_MIN_OFF_MS);
            }
        } else {
            // Запит на вимкнення — перевіряємо min ON time
            if (comp_since_ms_ < COMP_MIN_ON_MS) {
                out_.compressor = true;  // Тримаємо ON
                ESP_LOGD(TAG, "Compressor OFF blocked — min ON (%lu/%lu ms)",
                         comp_since_ms_, COMP_MIN_ON_MS);
            }
        }
    }

    // === ІНТЕРЛОКИ (hardcoded, неможливо обійти) ===
    // Виконуються ОСТАННІМИ — мають найвищий пріоритет після protection lockout.

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
    // Антикороткоциклування компресора — на рівні apply_arbitration()
    if (compressor_) {
        compressor_->set(out_.compressor);
    }
    if (heater_)   heater_->set(out_.heater);
    if (evap_fan_) evap_fan_->set(out_.evap_fan);
    if (cond_fan_) cond_fan_->set(out_.cond_fan);
    if (hg_valve_) hg_valve_->set(out_.hg_valve);
}

// ═══════════════════════════════════════════════════════════════
// Публікація фактичного стану актуаторів
// ═══════════════════════════════════════════════════════════════

void EquipmentModule::publish_state() {
    // AUDIT-002: публікуємо ФАКТИЧНИЙ стан реле (get_state()), а не бажаний (out_)
    bool comp_now = compressor_ ? compressor_->get_state() : false;

    // AUDIT-003: відстежуємо зміну фактичного стану компресора для таймера
    if (comp_now != comp_actual_) {
        comp_since_ms_ = 0;
        comp_actual_ = comp_now;
        ESP_LOGI(TAG, "Compressor → %s", comp_now ? "ON" : "OFF");
    }

    state_set("equipment.compressor", comp_now);
    state_set("equipment.heater",     heater_     ? heater_->get_state()     : false);
    state_set("equipment.evap_fan",   evap_fan_   ? evap_fan_->get_state()   : false);
    state_set("equipment.cond_fan",   cond_fan_   ? cond_fan_->get_state()   : false);
    state_set("equipment.hg_valve",   hg_valve_   ? hg_valve_->get_state()   : false);
}
