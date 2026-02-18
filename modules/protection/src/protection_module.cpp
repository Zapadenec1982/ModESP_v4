/**
 * @file protection_module.cpp
 * @brief Protection module — alarm monitoring and signaling (spec_v3 §2.7)
 *
 * Monitors:
 *   1. High Temp (HAL) — delayed (dAd), blocked during defrost
 *   2. Low Temp (LAL)  — delayed (dAd), always active
 *   3. Sensor1 (ERR1)  — instant, air sensor failure
 *   4. Sensor2 (ERR2)  — instant, evap sensor failure (info only)
 *   5. Door open       — delayed (door_delay), info only
 *
 * protection.lockout = false (reserved for Phase 10+)
 */

#include "protection_module.h"
#include "esp_log.h"

static const char* TAG = "Protection";

// ═══════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════

ProtectionModule::ProtectionModule()
    : BaseModule("protection", modesp::ModulePriority::HIGH)
{}

// ═══════════════════════════════════════════════════════════════
// Helpers: читання з SharedState
// ═══════════════════════════════════════════════════════════════

float ProtectionModule::read_float(const char* key, float def) {
    auto v = state_get(key);
    if (!v.has_value()) return def;
    const auto* fp = etl::get_if<float>(&v.value());
    return fp ? *fp : def;
}

bool ProtectionModule::read_bool(const char* key, bool def) {
    auto v = state_get(key);
    if (!v.has_value()) return def;
    const auto* bp = etl::get_if<bool>(&v.value());
    return bp ? *bp : def;
}

int32_t ProtectionModule::read_int(const char* key, int32_t def) {
    auto v = state_get(key);
    if (!v.has_value()) return def;
    const auto* ip = etl::get_if<int32_t>(&v.value());
    return ip ? *ip : def;
}

// ═══════════════════════════════════════════════════════════════
// Sync settings з SharedState (WebUI/API може їх змінити)
// ═══════════════════════════════════════════════════════════════

void ProtectionModule::sync_settings() {
    high_limit_ = read_float("protection.high_limit", high_limit_);
    low_limit_  = read_float("protection.low_limit", low_limit_);

    // Хвилини → мілісекунди
    alarm_delay_ms_ = static_cast<uint32_t>(read_int("protection.alarm_delay", 30)) * 60000;
    door_delay_ms_  = static_cast<uint32_t>(read_int("protection.door_delay", 5)) * 60000;

    manual_reset_ = read_bool("protection.manual_reset", manual_reset_);
}

// ═══════════════════════════════════════════════════════════════
// Lifecycle: on_init
// ═══════════════════════════════════════════════════════════════

bool ProtectionModule::on_init() {
    // PersistService вже відновив збережені значення з NVS → SharedState (Phase 1)
    sync_settings();

    // Початковий стан
    state_set("protection.lockout", false);
    state_set("protection.alarm_active", false);
    state_set("protection.alarm_code", "none");
    state_set("protection.high_temp_alarm", false);
    state_set("protection.low_temp_alarm", false);
    state_set("protection.sensor1_alarm", false);
    state_set("protection.sensor2_alarm", false);
    state_set("protection.door_alarm", false);
    state_set("protection.reset_alarms", false);

    ESP_LOGI(TAG, "Initialized (HAL=%.1f°C, LAL=%.1f°C, delay=%lu min)",
             high_limit_, low_limit_, alarm_delay_ms_ / 60000);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// Lifecycle: on_update — головний цикл
// ═══════════════════════════════════════════════════════════════

void ProtectionModule::on_update(uint32_t dt_ms) {
    // 1. Sync settings
    sync_settings();

    // 2. Читаємо inputs з SharedState
    float air_temp   = read_float("equipment.air_temp");
    bool  sensor1_ok = read_bool("equipment.sensor1_ok");
    bool  sensor2_ok = read_bool("equipment.sensor2_ok");
    bool  door_open  = read_bool("equipment.door_open");
    bool  defrost    = read_bool("defrost.active");

    // 3. Перевіряємо команду скидання аварій
    check_reset_command();

    // 4. Оновлюємо монітори
    update_high_temp(air_temp, sensor1_ok, defrost, dt_ms);
    update_low_temp(air_temp, sensor1_ok, dt_ms);
    update_sensor_alarm(sensor1_, sensor1_ok, "SENSOR1 (ERR1)");
    update_sensor_alarm(sensor2_, sensor2_ok, "SENSOR2 (ERR2)");
    update_door_alarm(door_open, dt_ms);

    // 5. Публікуємо стан аварій
    publish_alarms();
}

// ═══════════════════════════════════════════════════════════════
// High Temp alarm з dAd затримкою і defrost blocking
// ═══════════════════════════════════════════════════════════════

void ProtectionModule::update_high_temp(float temp, bool sensor_ok,
                                         bool defrost_active, uint32_t dt_ms) {
    // Блокується під час defrost (spec_v3 §1.3)
    if (defrost_active) {
        high_temp_.pending = false;
        high_temp_.pending_ms = 0;
        // НЕ скидаємо active якщо вже в аварії — тільки pending
        return;
    }

    // Не можемо перевірити без датчика
    if (!sensor_ok) {
        high_temp_.pending = false;
        high_temp_.pending_ms = 0;
        return;
    }

    if (temp > high_limit_) {
        // Вище межі
        if (!high_temp_.active) {
            high_temp_.pending = true;
            high_temp_.pending_ms += dt_ms;
            if (high_temp_.pending_ms >= alarm_delay_ms_) {
                high_temp_.active = true;
                high_temp_.pending = false;
                ESP_LOGW(TAG, "HIGH TEMP ALARM: %.1f > %.1f (delay %lu min)",
                         temp, high_limit_, alarm_delay_ms_ / 60000);
            }
        }
    } else {
        // Повернувся в норму
        high_temp_.pending = false;
        high_temp_.pending_ms = 0;
        if (high_temp_.active && !manual_reset_) {
            high_temp_.active = false;
            ESP_LOGI(TAG, "High temp alarm cleared (auto)");
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// Low Temp alarm з dAd затримкою (завжди активний)
// ═══════════════════════════════════════════════════════════════

void ProtectionModule::update_low_temp(float temp, bool sensor_ok, uint32_t dt_ms) {
    // Не можемо перевірити без датчика
    if (!sensor_ok) {
        low_temp_.pending = false;
        low_temp_.pending_ms = 0;
        return;
    }

    if (temp < low_limit_) {
        // Нижче межі
        if (!low_temp_.active) {
            low_temp_.pending = true;
            low_temp_.pending_ms += dt_ms;
            if (low_temp_.pending_ms >= alarm_delay_ms_) {
                low_temp_.active = true;
                low_temp_.pending = false;
                ESP_LOGW(TAG, "LOW TEMP ALARM: %.1f < %.1f (delay %lu min)",
                         temp, low_limit_, alarm_delay_ms_ / 60000);
            }
        }
    } else {
        // Повернувся в норму
        low_temp_.pending = false;
        low_temp_.pending_ms = 0;
        if (low_temp_.active && !manual_reset_) {
            low_temp_.active = false;
            ESP_LOGI(TAG, "Low temp alarm cleared (auto)");
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// Sensor alarm — instant (без затримки)
// ═══════════════════════════════════════════════════════════════

void ProtectionModule::update_sensor_alarm(AlarmMonitor& m, bool sensor_ok,
                                            const char* label) {
    if (!sensor_ok) {
        if (!m.active) {
            m.active = true;
            ESP_LOGW(TAG, "%s ALARM — sensor failure", label);
        }
    } else {
        if (m.active && !manual_reset_) {
            m.active = false;
            ESP_LOGI(TAG, "%s alarm cleared (auto)", label);
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// Door alarm з затримкою
// ═══════════════════════════════════════════════════════════════

void ProtectionModule::update_door_alarm(bool door_open, uint32_t dt_ms) {
    if (door_open) {
        if (!door_.active) {
            door_.pending = true;
            door_.pending_ms += dt_ms;
            if (door_.pending_ms >= door_delay_ms_) {
                door_.active = true;
                door_.pending = false;
                ESP_LOGW(TAG, "DOOR ALARM — open > %lu min", door_delay_ms_ / 60000);
            }
        }
    } else {
        // Двері закриті — скидаємо
        door_.pending = false;
        door_.pending_ms = 0;
        if (door_.active && !manual_reset_) {
            door_.active = false;
            ESP_LOGI(TAG, "Door alarm cleared (auto)");
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// Команда скидання аварій (manual reset через WebUI/API)
// ═══════════════════════════════════════════════════════════════

void ProtectionModule::check_reset_command() {
    if (read_bool("protection.reset_alarms")) {
        // Скидаємо всі активні аварії
        if (high_temp_.active || low_temp_.active || sensor1_.active ||
            sensor2_.active || door_.active) {
            high_temp_.active = false;
            high_temp_.pending = false;
            high_temp_.pending_ms = 0;
            low_temp_.active = false;
            low_temp_.pending = false;
            low_temp_.pending_ms = 0;
            sensor1_.active = false;
            sensor2_.active = false;
            door_.active = false;
            door_.pending = false;
            door_.pending_ms = 0;
            ESP_LOGI(TAG, "All alarms reset (manual)");
        }
        // Скидаємо trigger
        state_set("protection.reset_alarms", false);
    }
}

// ═══════════════════════════════════════════════════════════════
// Публікація стану аварій
// ═══════════════════════════════════════════════════════════════

void ProtectionModule::publish_alarms() {
    // lockout — зарезервовано (завжди false)
    state_set("protection.lockout", false);

    // Окремі алерти
    state_set("protection.high_temp_alarm", high_temp_.active);
    state_set("protection.low_temp_alarm", low_temp_.active);
    state_set("protection.sensor1_alarm", sensor1_.active);
    state_set("protection.sensor2_alarm", sensor2_.active);
    state_set("protection.door_alarm", door_.active);

    // Зведений статус
    bool any_alarm = high_temp_.active || low_temp_.active ||
                     sensor1_.active || sensor2_.active || door_.active;
    state_set("protection.alarm_active", any_alarm);

    // Код найвищої за пріоритетом аварії
    if (sensor1_.active) {
        alarm_code_ = "err1";
    } else if (high_temp_.active) {
        alarm_code_ = "high_temp";
    } else if (low_temp_.active) {
        alarm_code_ = "low_temp";
    } else if (sensor2_.active) {
        alarm_code_ = "err2";
    } else if (door_.active) {
        alarm_code_ = "door";
    } else {
        alarm_code_ = "none";
    }
    state_set("protection.alarm_code", alarm_code_);
}

// ═══════════════════════════════════════════════════════════════
// Stop
// ═══════════════════════════════════════════════════════════════

void ProtectionModule::on_stop() {
    state_set("protection.lockout", false);
    state_set("protection.alarm_active", false);
    state_set("protection.alarm_code", "none");
    ESP_LOGI(TAG, "Protection stopped");
}
