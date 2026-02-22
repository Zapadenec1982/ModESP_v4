/**
 * @file driver_manager.cpp
 * @brief DriverManager implementation — driver creation from bindings
 *
 * Includes concrete driver headers and manages static driver pools.
 * Each binding maps a hardware resource to a driver type and role.
 *
 * Driver types:
 *   "ds18b20"       → DS18B20Driver       (sensor, OneWire)
 *   "digital_input" → DigitalInputDriver   (sensor, GPIO)
 *   "ntc"           → NtcDriver            (sensor, ADC)
 *   "relay"         → RelayDriver          (actuator, GPIO)
 */

#include "modesp/hal/driver_manager.h"
#include "ds18b20_driver.h"
#include "relay_driver.h"
#include "digital_input_driver.h"
#include "ntc_driver.h"
#include "esp_log.h"

static const char* TAG = "DriverMgr";

namespace modesp {

// ═══════════════════════════════════════════════════════════════
// Static driver pools — zero heap allocation
// ═══════════════════════════════════════════════════════════════

static DS18B20Driver ds18b20_pool[MAX_SENSORS];
static size_t ds18b20_count = 0;

static DigitalInputDriver di_pool[MAX_SENSORS];
static size_t di_count = 0;

static NtcDriver ntc_pool[MAX_SENSORS];
static size_t ntc_count = 0;

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
    di_count = 0;
    ntc_count = 0;
    relay_count = 0;
    sensors_.clear();
    actuators_.clear();
    sensor_count_ = 0;
    actuator_count_ = 0;

    // Лямбда для реєстрації сенсора
    auto add_sensor = [this](ISensorDriver* drv, const Binding& b) -> bool {
        if (!drv) return false;
        SensorEntry entry;
        entry.driver = drv;
        entry.role = b.role;
        entry.module = b.module_name;
        sensors_.push_back(entry);
        sensor_count_++;
        ESP_LOGI(TAG, "  Sensor '%s' [%s] -> module '%s'",
                 b.role.c_str(), b.driver_type.c_str(), b.module_name.c_str());
        return true;
    };

    // Phase 1: Create drivers from bindings
    for (const auto& binding : bindings.bindings) {
        if (binding.driver_type == "ds18b20") {
            if (!add_sensor(create_sensor(binding, hal), binding)) {
                ESP_LOGE(TAG, "  Failed to create sensor '%s'", binding.role.c_str());
                return false;
            }
        } else if (binding.driver_type == "digital_input") {
            if (!add_sensor(create_di_sensor(binding, hal), binding)) {
                ESP_LOGE(TAG, "  Failed to create DI sensor '%s'", binding.role.c_str());
                return false;
            }
        } else if (binding.driver_type == "ntc") {
            if (!add_sensor(create_ntc_sensor(binding, hal), binding)) {
                ESP_LOGE(TAG, "  Failed to create NTC sensor '%s'", binding.role.c_str());
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
        drv.configure(binding.role.c_str(), ow_res->gpio, 1000,
                      binding.address.empty() ? nullptr : binding.address.c_str());
        return &drv;
    }

    return nullptr;
}

ISensorDriver* DriverManager::create_di_sensor(const Binding& binding, HAL& hal) {
    if (di_count >= MAX_SENSORS) {
        ESP_LOGE(TAG, "DigitalInput pool exhausted");
        return nullptr;
    }

    auto* gpio_res = hal.find_gpio_input(
        etl::string_view(binding.hardware_id.c_str(), binding.hardware_id.size()));
    if (!gpio_res) {
        ESP_LOGE(TAG, "GPIO input '%s' not found in HAL", binding.hardware_id.c_str());
        return nullptr;
    }

    auto& drv = di_pool[di_count++];
    drv.configure(binding.role.c_str(), gpio_res->gpio, gpio_res->pull_up);
    return &drv;
}

ISensorDriver* DriverManager::create_ntc_sensor(const Binding& binding, HAL& hal) {
    if (ntc_count >= MAX_SENSORS) {
        ESP_LOGE(TAG, "NTC pool exhausted");
        return nullptr;
    }

    auto* adc_res = hal.find_adc_channel(
        etl::string_view(binding.hardware_id.c_str(), binding.hardware_id.size()));
    if (!adc_res) {
        ESP_LOGE(TAG, "ADC channel '%s' not found in HAL", binding.hardware_id.c_str());
        return nullptr;
    }

    auto& drv = ntc_pool[ntc_count++];
    drv.configure(binding.role.c_str(), adc_res->gpio, adc_res->atten);
    return &drv;
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
        // min_switch_ms = 0 для всіх реле. Захист компресора від коротких циклів
        // реалізовано на рівні EquipmentModule (COMP_MIN_OFF_MS / COMP_MIN_ON_MS)
        // з асиметричними таймерами (180с OFF, 120с ON).
        drv.configure(binding.role.c_str(), gpio_res->gpio, gpio_res->active_high, 0);
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
