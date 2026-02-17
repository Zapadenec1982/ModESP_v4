# ModESP v4 — Приклади модулів

## Приклад 1: DS18B20 Driver

Драйвер температурного датчика — типовий модуль-джерело даних.

```cpp
// drivers/ds18b20/include/ds18b20_messages.h
#pragma once
#include "modesp/types.h"

struct MsgSensorReading : etl::message<modesp::msg_id::SENSOR_READING> {
    etl::string<16> sensor_id;
    float value;
    uint64_t timestamp_us;
};

struct MsgSensorError : etl::message<modesp::msg_id::SENSOR_ERROR> {
    etl::string<16> sensor_id;
    int32_t error_code;
};
```

```cpp
// drivers/ds18b20/include/ds18b20_driver.h
#pragma once
#include "modesp/base_module.h"
#include "ds18b20_messages.h"

class DS18B20Driver : public modesp::BaseModule {
public:
    DS18B20Driver(uint8_t gpio_pin)
        : BaseModule("ds18b20", modesp::ModulePriority::HIGH)
        , gpio_(gpio_pin) {}

    bool on_init() override {
        // Ініціалізація OneWire шини
        return onewire_init(gpio_);
    }

    void on_update(uint32_t dt_ms) override {
        ms_since_read_ += dt_ms;
        if (ms_since_read_ < read_interval_ms_) return;
        ms_since_read_ = 0;

        float temp;
        if (retry([&]() { return read_raw_temperature(temp); })) {
            // Валідація
            if (temp < -55.0f || temp > 125.0f) {
                // Невалідне значення DS18B20
                error_count_++;
                return;
            }

            current_temp_ = temp;
            error_count_ = 0;

            // Записати в SharedState
            state_set_float("sensors.temp1", temp);

            // Опублікувати на шину
            MsgSensorReading msg;
            msg.sensor_id = "temp1";
            msg.value = temp;
            msg.timestamp_us = esp_timer_get_time();
            publish(msg);  // zero allocation
        } else {
            error_count_++;
            if (error_count_ >= 5) {
                MsgSensorError err;
                err.sensor_id = "temp1";
                err.error_code = -1;
                publish(err);
            }
        }
    }

private:
    uint8_t gpio_;
    float current_temp_ = 0;
    uint32_t ms_since_read_ = 0;
    uint32_t read_interval_ms_ = 1000;  // раз на секунду
    uint8_t error_count_ = 0;
};
```

## Приклад 2: Thermostat Module

Бізнес-логіка — слухає температуру, керує компресором.

```cpp
// modules/thermostat/include/thermostat_module.h
#pragma once
#include "modesp/base_module.h"
#include "ds18b20_messages.h"   // тільки те, що слухаємо
#include "relay_messages.h"      // те, що відправляємо

class ThermostatModule : public modesp::BaseModule {
public:
    ThermostatModule()
        : BaseModule("thermostat", modesp::ModulePriority::NORMAL) {}

    bool on_init() override {
        // Прочитати setpoint з конфігурації (якщо є)
        auto sp = state_get_float("config.setpoint");
        if (sp.has_value()) setpoint_ = sp.value();
        return true;
    }

    void on_message(const etl::imessage& msg) override {
        if (msg.get_message_id() == modesp::msg_id::SENSOR_READING) {
            auto& reading = static_cast<const MsgSensorReading&>(msg);
            if (reading.sensor_id == "temp1") {
                current_temp_ = reading.value;
            }
        }
        // Якщо прийшла зміна setpoint через WebSocket:
        if (msg.get_message_id() == modesp::msg_id::SETPOINT_CHANGED) {
            // ...
        }
    }

    void on_update(uint32_t dt_ms) override {
        // On/Off control з гістерезисом
        bool should_cool = current_temp_ > (setpoint_ + hysteresis_);
        bool should_stop = current_temp_ < (setpoint_ - hysteresis_);

        if (should_cool && !compressor_on_) {
            // Захист мінімального часу зупинки компресора
            if (off_time_ms_ >= min_off_time_ms_) {
                compressor_on_ = true;
                MsgActuatorCommand cmd;
                cmd.actuator_id = "compressor";
                cmd.state = true;
                publish(cmd);
            }
        } else if (should_stop && compressor_on_) {
            compressor_on_ = false;
            off_time_ms_ = 0;
            MsgActuatorCommand cmd;
            cmd.actuator_id = "compressor";
            cmd.state = false;
            publish(cmd);
        }

        // Лічильники
        if (!compressor_on_) off_time_ms_ += dt_ms;
        state_set_bool("actuator.compressor", compressor_on_);
    }

private:
    float setpoint_    = -18.0f;  // °C (морозильник)
    float hysteresis_  = 1.5f;    // °C
    float current_temp_ = 0;
    bool  compressor_on_ = false;
    uint32_t off_time_ms_ = 999999;  // Великий, щоб дозволити старт
    uint32_t min_off_time_ms_ = 180000;  // 3 хвилини захист компресора
};
```

## Приклад 3: main.cpp — Збірка системи

```cpp
#include "modesp/core.h"

// Системні сервіси
#include "modesp/services/error_service.h"
#include "modesp/services/watchdog_service.h"
#include "modesp/services/config_service.h"
#include "modesp/services/persist_service.h"
#include "modesp/services/logger_service.h"
#include "modesp/services/system_monitor.h"

// Драйвери
#include "ds18b20_driver.h"
#include "relay_driver.h"

// Бізнес-логіка
#include "thermostat_module.h"

// ═══ Static instances (zero heap) ═══
// Системні сервіси
static modesp::ErrorService    error_svc;
static modesp::WatchdogService watchdog_svc;
static modesp::ConfigService   config_svc;
static modesp::PersistService  persist_svc;
static modesp::LoggerService   logger_svc;
static modesp::SystemMonitor   sysmon_svc;

// Драйвери
static DS18B20Driver temp_sensor(GPIO_NUM_4);
static RelayDriver   compressor(GPIO_NUM_16, "compressor");

// Бізнес-логіка
static ThermostatModule thermostat;

extern "C" void app_main() {
    auto& app = modesp::App::instance();
    app.init();

    // Сервіси (CRITICAL/HIGH — перші)
    app.modules().register_module(error_svc);
    app.modules().register_module(watchdog_svc);
    app.modules().register_module(config_svc);
    app.modules().register_module(persist_svc);
    app.modules().register_module(logger_svc);
    app.modules().register_module(sysmon_svc);

    // Драйвери
    app.modules().register_module(temp_sensor);
    app.modules().register_module(compressor);

    // Бізнес-логіка
    app.modules().register_module(thermostat);

    // Запуск (блокує назавжди)
    app.run();
}
```

**Порядок update() визначається пріоритетом, не порядком реєстрації:**
```
1. CRITICAL: error_svc, watchdog_svc
2. HIGH:     config_svc, temp_sensor, compressor
3. NORMAL:   thermostat
4. LOW:      persist_svc, logger_svc, sysmon_svc
```
