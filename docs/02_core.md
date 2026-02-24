# ModESP v4 — Core (modesp_core)

Ядро системи. Не залежить від нічого крім ETL та FreeRTOS.
Не знає про конкретні модулі, драйвери чи бізнес-логіку.

## types.h — Базові типи

```cpp
#pragma once
#include "etl/string.h"
#include "etl/variant.h"
#include "etl/optional.h"
#include "etl/message.h"

namespace modesp {

// ─── Ключ стану: фіксований рядок, zero heap ───
using StateKey = etl::string<32>;

// ─── Значення стану: один з фіксованих типів ───
using StateValue = etl::variant<
    int32_t,
    float,
    bool,
    etl::string<32>
>;

// ─── Пріоритет модуля: визначає порядок init/update ───
enum class ModulePriority : uint8_t {
    CRITICAL = 0,  // error_service, watchdog — перші init, перші update
    HIGH     = 1,  // config, сенсори, актуатори
    NORMAL   = 2,  // бізнес-логіка: thermostat, protection, defrost
    LOW      = 3,  // logger, system_monitor, persist, datalogger
};

// ─── ID повідомлень ───
// Системні:       0-49
// Сервіси:       50-99
// HAL:          100-109
// Драйвери:     110-149
// Модулі:       150-249
namespace msg_id {
    // Системні (core)
    constexpr etl::message_id_t SYSTEM_INIT       = 0;
    constexpr etl::message_id_t SYSTEM_SHUTDOWN    = 1;
    constexpr etl::message_id_t SYSTEM_ERROR       = 2;
    constexpr etl::message_id_t SYSTEM_HEALTH      = 3;
    constexpr etl::message_id_t STATE_CHANGED      = 4;
    constexpr etl::message_id_t CONFIG_CHANGED     = 5;
    constexpr etl::message_id_t TIMER_TICK         = 6;
    constexpr etl::message_id_t SYSTEM_SAFE_MODE   = 7;

    // Сервіси
    constexpr etl::message_id_t MODULE_TIMEOUT     = 50;
    constexpr etl::message_id_t CONFIG_LOADED      = 51;
    constexpr etl::message_id_t CONFIG_SAVED       = 52;
    constexpr etl::message_id_t CONFIG_RESET       = 53;
    constexpr etl::message_id_t LOG_ENTRY          = 54;
    constexpr etl::message_id_t PERSIST_SAVED      = 55;

    // HAL
    constexpr etl::message_id_t GPIO_CHANGED       = 100;

    // Драйвери
    constexpr etl::message_id_t SENSOR_READING     = 110;
    constexpr etl::message_id_t SENSOR_ERROR       = 111;
    constexpr etl::message_id_t ACTUATOR_COMMAND   = 120;
    constexpr etl::message_id_t ACTUATOR_FEEDBACK  = 121;

    // Модулі
    constexpr etl::message_id_t ALARM_TRIGGERED    = 150;
    constexpr etl::message_id_t ALARM_CLEARED      = 151;
    constexpr etl::message_id_t SETPOINT_CHANGED   = 160;
    constexpr etl::message_id_t DEFROST_START      = 170;
    constexpr etl::message_id_t DEFROST_END        = 171;
}

} // namespace modesp
```

## base_module.h — Базовий клас модуля

Кожен модуль (драйвер, сервіс, бізнес-логіка) наслідує BaseModule.

```cpp
#pragma once
#include "modesp/types.h"
#include "etl/imessage.h"

namespace modesp {

class ModuleManager;  // forward declaration

class BaseModule {
public:
    BaseModule(const char* name, ModulePriority priority = ModulePriority::NORMAL)
        : name_(name), priority_(priority) {}
    virtual ~BaseModule() = default;

    // ─── Lifecycle (реалізується модулем) ───
    virtual bool on_init()                          { return true; }
    virtual void on_start()                         {}
    virtual void on_update(uint32_t dt_ms)          {}
    virtual void on_stop()                          {}
    virtual void on_message(const etl::imessage&)   {}

    // ─── API (доступний модулю після реєстрації) ───
    void publish(const etl::imessage& msg);
    bool state_set(const StateKey& key, const StateValue& value);
    etl::optional<StateValue> state_get(const StateKey& key) const;

    // Typed helpers для зручності
    bool state_set_float(const char* key, float value);
    bool state_set_int(const char* key, int32_t value);
    bool state_set_bool(const char* key, bool value);
    etl::optional<float> state_get_float(const char* key) const;
    etl::optional<int32_t> state_get_int(const char* key) const;
    etl::optional<bool> state_get_bool(const char* key) const;

    // ─── Features (Phase 10.5) ───
    bool has_feature(const char* feature_name) const;  // constexpr lookup via features_config.h

    // ─── Метадані ───
    const char*    name()     const { return name_; }
    ModulePriority priority() const { return priority_; }
    bool           enabled()  const { return enabled_; }
    void           set_enabled(bool e) { enabled_ = e; }

    // ─── Heartbeat (оновлюється ModuleManager) ───
    uint32_t last_update_ms() const { return last_update_ms_; }

private:
    friend class ModuleManager;  // MM встановлює manager_ при реєстрації

    const char* name_;
    ModulePriority priority_;
    bool enabled_ = true;
    uint32_t last_update_ms_ = 0;
    ModuleManager* manager_ = nullptr;  // Встановлюється при register_module()
};

} // namespace modesp
```

**Ключовий момент:** Модуль НЕ знає про App singleton.
Він отримує доступ до шини та стану через `manager_`,
який встановлюється при реєстрації. Це дозволяє тестувати
модулі окремо з mock ModuleManager.

## shared_state.h — Сховище стану

Thread-safe key-value store з фіксованим розміром.
O(1) average access, zero heap allocation.

```cpp
#pragma once
#include "modesp/types.h"
#include "etl/unordered_map.h"
#include "freertos/semphr.h"

namespace modesp {

template<size_t MAX_ENTRIES = 128>
class SharedState {
public:
    bool init() {
        mutex_ = xSemaphoreCreateMutex();
        return mutex_ != nullptr;
    }

    bool set(const StateKey& key, const StateValue& value) {
        ScopedLock lock(mutex_);
        if (!lock.acquired()) return false;
        store_[key] = value;
        return true;
    }

    etl::optional<StateValue> get(const StateKey& key) const {
        ScopedLock lock(mutex_);
        if (!lock.acquired()) return etl::nullopt;
        auto it = store_.find(key);
        if (it == store_.end()) return etl::nullopt;
        return it->second;
    }

    bool has(const StateKey& key) const {
        ScopedLock lock(mutex_);
        if (!lock.acquired()) return false;
        return store_.find(key) != store_.end();
    }

    size_t size() const {
        ScopedLock lock(mutex_);
        return store_.size();
    }

    size_t capacity() const { return MAX_ENTRIES; }

    // Ітерація (для серіалізації в JSON — cold path)
    using Callback = void(*)(const StateKey&, const StateValue&, void*);
    void for_each(Callback cb, void* user_data) const {
        ScopedLock lock(mutex_);
        if (!lock.acquired()) return;
        for (const auto& pair : store_) {
            cb(pair.first, pair.second, user_data);
        }
    }

private:
    etl::unordered_map<StateKey, StateValue, MAX_ENTRIES> store_;
    mutable SemaphoreHandle_t mutex_ = nullptr;

    struct ScopedLock {
        ScopedLock(SemaphoreHandle_t m) : m_(m) {
            ok_ = (xSemaphoreTake(m_, pdMS_TO_TICKS(10)) == pdTRUE);
        }
        ~ScopedLock() { if (ok_) xSemaphoreGive(m_); }
        bool acquired() const { return ok_; }
    private:
        SemaphoreHandle_t m_;
        bool ok_;
    };
};

} // namespace modesp
```

**RAM:** ~8 KB для 128 entries (StateKey=32B + StateValue≈36B = 68B × 128).

**version_ counter:** інкрементується на кожен `set()` — WsService порівнює версії для delta broadcast.

**Persist callback:** `set()` перевіряє зміну значення → викликає callback ПОЗА mutex для PersistService.

## module_manager.h — Управління модулями

Реєстрація, lifecycle, message bus.

```cpp
#pragma once
#include "modesp/types.h"
#include "modesp/base_module.h"
#include "modesp/shared_state.h"
#include "etl/message_bus.h"
#include "etl/vector.h"

namespace modesp {

class ModuleManager {
public:
    static constexpr size_t MAX_MODULES = 16;
    static constexpr size_t MAX_BUS_ROUTERS = 24;

    bool init();

    // ─── Реєстрація ───
    bool register_module(BaseModule& module);

    // ─── Lifecycle ───
    bool init_all();    // Викликає on_init() в порядку пріоритету
    void start_all();   // Викликає on_start()
    void update_all(uint32_t dt_ms);  // Викликає on_update()
    void stop_all();    // Викликає on_stop() в ЗВОРОТНОМУ порядку

    // ─── Message Bus ───
    void publish(const etl::imessage& msg);
    etl::imessage_bus& bus() { return bus_; }

    // ─── State ───
    SharedState<128>& state() { return state_; }

    // ─── Доступ до модулів ───
    BaseModule* find(const char* name);
    size_t count() const { return modules_.size(); }

private:
    etl::vector<BaseModule*, MAX_MODULES> modules_;
    etl::message_bus<MAX_BUS_ROUTERS> bus_;
    SharedState<128> state_;

    void sort_by_priority();
};

} // namespace modesp
```

## app.h — Application Lifecycle

App — зручна обгортка для main.cpp. 
**Модулі НЕ мають доступу до App** — вони працюють через BaseModule API.

```cpp
#pragma once
#include "modesp/module_manager.h"
#include "esp_task_wdt.h"

namespace modesp {

class App {
public:
    bool init();   // NVS, HW watchdog, ModuleManager
    void run();    // Блокуючий main loop

    ModuleManager& modules() { return mm_; }

    // Версіонування прошивки
    static constexpr const char* VERSION    = "4.0.0";
    static constexpr const char* BUILD_DATE = __DATE__ " " __TIME__;

private:
    ModuleManager mm_;

    // Main loop config (з Kconfig)
    static constexpr uint32_t LOOP_PERIOD_MS       = 10;   // 100 Hz
    static constexpr uint32_t UPDATE_BUDGET_MS      = 8;    // макс час на модулі
    static constexpr uint32_t HW_WDT_TIMEOUT_MS    = 10000; // HW watchdog
};

} // namespace modesp
```

**Hardware Watchdog:** App ініціалізує ESP32 Task WDT при старті
і скидає його кожен цикл main loop. Якщо main loop зависне — 
ESP32 автоматично перезавантажиться через 10 секунд.

Це **другий рівень захисту** поверх software WatchdogService.
