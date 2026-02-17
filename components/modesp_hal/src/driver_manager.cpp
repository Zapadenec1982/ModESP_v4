/**
 * @file driver_manager.cpp
 * @brief DriverManager implementation — driver creation from bindings
 *
 * Includes concrete driver headers and manages static driver pools.
 * Each binding maps a hardware resource to a driver type and role.
 *
 * Driver types:
 *   "ds18b20" → DS18B20Driver (sensor)
 *   "relay"   → RelayDriver   (actuator)
 */

#include "modesp/hal/driver_manager.h"
#include "ds18b20_driver.h"
#include "relay_driver.h"
#include "esp_log.h"

static const char* TAG = "DriverMgr";

namespace modesp {

// ═══════════════════════════════════════════════════════════════
// Static driver pools — zero heap allocation
// ═══════════════════════════════════════════════════════════════

static DS18B20Driver ds18b20_pool[MAX_SENSORS];
static size_t ds18b20_count = 0;

static RelayDriver relay_pool[MAX_ACTUATORS];
static size_t relay_count = 0;

// ═══════════════════════════════════════════════════════════════
// Init — create all drivers from bindings
// ═══════════════════════════════════════════════════════════════

bool DriverManager::init(const BindingTable& bindings, HAL& hal) {
    ESP_LOGI(TAG, "Creating drivers for %d bindings...",
             (int)bindings.bindings.size());

    // Reset pools
    ds18b20_count = 0;
    relay_count = 0;
    sensors_.clear();
    actuators_.clear();
    sensor_count_ = 0;
    actuator_count_ = 0;

    // Phase 1: Create drivers from bindings
    for (const auto& binding : bindings.bindings) {
        if (binding.driver_type == "ds18b20") {
            ISensorDriver* drv = create_sensor(binding, hal);
            if (drv) {
                SensorEntry entry;
                entry.driver = drv;
                entry.role = binding.role;
                entry.module = binding.module_name;
                sensors_.push_back(entry);
                sensor_count_++;

                ESP_LOGI(TAG, "  Sensor '%s' [%s] -> module '%s'",
                         binding.role.c_str(),
                         binding.driver_type.c_str(),
                         binding.module_name.c_str());
            } else {
                ESP_LOGE(TAG, "  Failed to create sensor '%s'",
                         binding.role.c_str());
                return false;
            }
        } else if (binding.driver_type == "relay") {
            IActuatorDriver* drv = create_actuator(binding, hal);
            if (drv) {
                ActuatorEntry entry;
                entry.driver = drv;
                entry.role = binding.role;
                entry.module = binding.module_name;
                actuators_.push_back(entry);
                actuator_count_++;

                ESP_LOGI(TAG, "  Actuator '%s' [%s] -> module '%s'",
                         binding.role.c_str(),
                         binding.driver_type.c_str(),
                         binding.module_name.c_str());
            } else {
                ESP_LOGE(TAG, "  Failed to create actuator '%s'",
                         binding.role.c_str());
                return false;
            }
        } else {
            ESP_LOGW(TAG, "  Unknown driver type '%s' for binding '%s'",
                     binding.driver_type.c_str(),
                     binding.hardware_id.c_str());
        }
    }

    // Phase 2: Initialize all created drivers
    for (auto& entry : sensors_) {
        if (!entry.driver->init()) {
            ESP_LOGE(TAG, "Sensor '%s' init failed", entry.role.c_str());
            return false;
        }
    }

    for (auto& entry : actuators_) {
        if (!entry.driver->init()) {
            ESP_LOGE(TAG, "Actuator '%s' init failed", entry.role.c_str());
            return false;
        }
    }

    ESP_LOGI(TAG, "DriverManager ready: %d sensors, %d actuators",
             (int)sensor_count_, (int)actuator_count_);
    return true;
}

// ═══════════════════════════════════════════════════════════════
// Driver creation
// ═══════════════════════════════════════════════════════════════

ISensorDriver* DriverManager::create_sensor(const Binding& binding, HAL& hal) {
    if (binding.driver_type == "ds18b20") {
        if (ds18b20_count >= MAX_SENSORS) {
            ESP_LOGE(TAG, "DS18B20 pool exhausted");
            return nullptr;
        }

        auto* ow_res = hal.find_onewire_bus(
            etl::string_view(binding.hardware_id.c_str(), binding.hardware_id.size()));
        if (!ow_res) {
            ESP_LOGE(TAG, "OneWire bus '%s' not found in HAL",
                     binding.hardware_id.c_str());
            return nullptr;
        }

        auto& drv = ds18b20_pool[ds18b20_count++];
        drv.configure(binding.role.c_str(), ow_res->gpio, 1000);
        return &drv;
    }

    return nullptr;
}

IActuatorDriver* DriverManager::create_actuator(const Binding& binding, HAL& hal) {
    if (binding.driver_type == "relay") {
        if (relay_count >= MAX_ACTUATORS) {
            ESP_LOGE(TAG, "Relay pool exhausted");
            return nullptr;
        }

        auto* gpio_res = hal.find_gpio_output(
            etl::string_view(binding.hardware_id.c_str(), binding.hardware_id.size()));
        if (!gpio_res) {
            ESP_LOGE(TAG, "GPIO output '%s' not found in HAL",
                     binding.hardware_id.c_str());
            return nullptr;
        }

        auto& drv = relay_pool[relay_count++];
        drv.configure(binding.role.c_str(), gpio_res->gpio, gpio_res->active_high, 180000);
        return &drv;
    }

    return nullptr;
}

// ═══════════════════════════════════════════════════════════════
// Lookup
// ═══════════════════════════════════════════════════════════════

ISensorDriver* DriverManager::find_sensor(etl::string_view role) {
    for (auto& entry : sensors_) {
        if (entry.role.size() == role.size() &&
            etl::string_view(entry.role.c_str(), entry.role.size()) == role) {
            return entry.driver;
        }
    }
    return nullptr;
}

IActuatorDriver* DriverManager::find_actuator(etl::string_view role) {
    for (auto& entry : actuators_) {
        if (entry.role.size() == role.size() &&
            etl::string_view(entry.role.c_str(), entry.role.size()) == role) {
            return entry.driver;
        }
    }
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════
// Update all drivers
// ═══════════════════════════════════════════════════════════════

void DriverManager::update_all(uint32_t dt_ms) {
    for (auto& entry : sensors_) {
        entry.driver->update(dt_ms);
    }
    for (auto& entry : actuators_) {
        entry.driver->update(dt_ms);
    }
}

} // namespace modesp
