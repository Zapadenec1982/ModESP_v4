/**
 * @file thermostat_module.cpp
 * @brief Thermostat with on/off hysteresis regulation
 *
 * Hysteresis algorithm:
 *   temp > setpoint + hysteresis  ->  compressor ON  (cooling needed)
 *   temp < setpoint - hysteresis  ->  compressor OFF (target reached)
 *   else                          ->  no change      (dead zone)
 *
 * Compressor protection:
 *   After turning off, must wait MIN_OFF_TIME_MS before next start.
 *   This prevents short-cycling the compressor.
 *
 * Temperature is read directly from ISensorDriver (no message bus).
 * Relay is controlled directly via IActuatorDriver::set().
 */

#include "thermostat_module.h"
#include "modesp/hal/driver_manager.h"
#include "modesp/driver_messages.h"
#include "esp_log.h"

static const char* TAG = "Thermostat";

// ═══════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════

ThermostatModule::ThermostatModule()
    : BaseModule("thermostat", modesp::ModulePriority::NORMAL)
{}

// ═══════════════════════════════════════════════════════════════
// Driver binding (called from main before init)
// ═══════════════════════════════════════════════════════════════

void ThermostatModule::bind_drivers(modesp::DriverManager& dm) {
    temp_sensor_ = dm.find_sensor("chamber_temp");
    compressor_  = dm.find_actuator("compressor");

    if (!temp_sensor_) {
        ESP_LOGE(TAG, "Sensor 'chamber_temp' not found in DriverManager");
    }
    if (!compressor_) {
        ESP_LOGE(TAG, "Actuator 'compressor' not found in DriverManager");
    }
}

// ═══════════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════════

bool ThermostatModule::on_init() {
    if (!temp_sensor_ || !compressor_) {
        ESP_LOGE(TAG, "Drivers not bound — call bind_drivers() first");
        return false;
    }

    // PersistService вже відновив збережені значення з NVS → SharedState (Phase 1)
    // Використовуємо persisted значення якщо є, інакше — defaults з коду
    auto sp = state_get("thermostat.setpoint");
    if (sp.has_value()) {
        const auto* fp = etl::get_if<float>(&sp.value());
        if (fp) {
            setpoint_ = *fp;
            ESP_LOGI(TAG, "Setpoint from NVS: %.1f°C", setpoint_);
        }
    }
    auto hy = state_get("thermostat.hysteresis");
    if (hy.has_value()) {
        const auto* fp = etl::get_if<float>(&hy.value());
        if (fp) {
            hysteresis_ = *fp;
            ESP_LOGI(TAG, "Hysteresis from NVS: %.1f°C", hysteresis_);
        }
    }

    // Публікуємо початковий стан в SharedState
    state_set("thermostat.temperature", 0.0f);
    state_set("thermostat.setpoint", setpoint_);
    state_set("thermostat.hysteresis", hysteresis_);
    state_set("thermostat.compressor", false);
    state_set("thermostat.state", "idle");

    ESP_LOGI(TAG, "Initialized (setpoint=%.1f°C, hysteresis=%.1f°C, min_off=%lu ms)",
             setpoint_, hysteresis_, MIN_OFF_TIME_MS);
    return true;
}

void ThermostatModule::on_update(uint32_t dt_ms) {
    uptime_ms_ += dt_ms;

    // Track compressor off time
    if (!compressor_on_) {
        off_time_ms_ += dt_ms;
    }

    // Перечитуємо setpoint/hysteresis з SharedState (змінені через WebUI/API)
    auto sp = state_get("thermostat.setpoint");
    if (sp.has_value()) {
        const auto* fp = etl::get_if<float>(&sp.value());
        if (fp && *fp != setpoint_) {
            setpoint_ = *fp;
            ESP_LOGI(TAG, "Setpoint updated: %.1f°C", setpoint_);
        }
    }
    auto hy = state_get("thermostat.hysteresis");
    if (hy.has_value()) {
        const auto* fp = etl::get_if<float>(&hy.value());
        if (fp && *fp != hysteresis_) {
            hysteresis_ = *fp;
            ESP_LOGI(TAG, "Hysteresis updated: %.1f°C", hysteresis_);
        }
    }

    // Читаємо температуру з датчика і публікуємо в SharedState
    float temp = 0.0f;
    if (temp_sensor_->read(temp)) {
        current_temp_ = temp;
        has_temp_ = true;
        state_set("thermostat.temperature", current_temp_);
    }

    // No temperature reading yet — nothing to regulate
    if (!has_temp_) return;

    // Safe mode — compressor already stopped
    if (safe_mode_) return;

    // On/off regulation with hysteresis
    bool should_cool = current_temp_ > (setpoint_ + hysteresis_);
    bool should_stop = current_temp_ < (setpoint_ - hysteresis_);

    if (should_cool && !compressor_on_) {
        // Check minimum off time protection
        if (off_time_ms_ >= MIN_OFF_TIME_MS) {
            if (compressor_->set(true)) {
                compressor_on_ = true;
                state_set("thermostat.state", "cooling");
                state_set("thermostat.compressor", true);
                ESP_LOGI(TAG, "Compressor ON (temp=%.1f°C > %.1f°C)",
                         current_temp_, setpoint_ + hysteresis_);
            } else {
                ESP_LOGW(TAG, "Compressor ON rejected by driver");
            }
        } else {
            ESP_LOGD(TAG, "Compressor start delayed (off_time=%lu/%lu ms)",
                     off_time_ms_, MIN_OFF_TIME_MS);
        }
    } else if (should_stop && compressor_on_) {
        compressor_->set(false);
        compressor_on_ = false;
        off_time_ms_ = 0;

        state_set("thermostat.state", "idle");
        state_set("thermostat.compressor", false);
        ESP_LOGI(TAG, "Compressor OFF (temp=%.1f°C < %.1f°C)",
                 current_temp_, setpoint_ - hysteresis_);
    }
}

void ThermostatModule::on_message(const etl::imessage& msg) {
    // Setpoint change (from WebSocket, config, etc.)
    if (msg.get_message_id() == modesp::msg_id::SETPOINT_CHANGED) {
        const auto& sp_msg = static_cast<const modesp::MsgSetpointChanged&>(msg);
        if (sp_msg.target == "thermostat") {
            setpoint_ = sp_msg.value;
            state_set("thermostat.setpoint", setpoint_);
            ESP_LOGI(TAG, "Setpoint changed: %.1f°C", setpoint_);
        }
        return;
    }

    // Safe Mode: stop regulation
    if (msg.get_message_id() == modesp::msg_id::SYSTEM_SAFE_MODE) {
        safe_mode_ = true;
        if (compressor_on_) {
            compressor_->emergency_stop();
            compressor_on_ = false;
            off_time_ms_ = 0;
            state_set("thermostat.state", "safe_mode");
            state_set("thermostat.compressor", false);
            ESP_LOGW(TAG, "SAFE MODE — thermostat stopped");
        }
        return;
    }
}

void ThermostatModule::on_stop() {
    // Turn off compressor on shutdown
    if (compressor_on_ && compressor_) {
        compressor_->set(false);
        compressor_on_ = false;
    }
    ESP_LOGI(TAG, "Thermostat stopped");
}
