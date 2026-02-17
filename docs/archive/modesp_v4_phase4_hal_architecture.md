# ModESP v4 — Phase 4: HAL + Configuration Architecture

## Мета документу

Повна архітектура Hardware Abstraction Layer, системи конфігурації та динамічного зв'язування hardware↔software для ModESP v4. Цей документ є основою для імплементації Phase 4.

**Попередні фази:**
- Phase 1: modesp_core (BaseModule, SharedState, ModuleManager, App, LED blink) ✅
- Phase 2: modesp_services (ErrorService, WatchdogService, LoggerService, SystemMonitor) ✅
- Phase 3: Drivers + Thermostat (DS18B20, Relay, ThermostatModule) ✅
- **Phase 4: HAL + ConfigService + DriverManager + рефакторинг модулів** ← ЦЕ

---

## Ключова концепція

ModESP — це платформа для різних пристроїв та плат. Прошивка конфігурується через JSON файли, які визначають:
- **board.json** — яке hardware є на платі (GPIO, шини, реле, датчики)
- **bindings.json** — яке hardware виконує яку функцію (relay_1 = compressor)

Оператор через WebUI може перепризначити hardware↔функція без перекомпіляції.

Модулі бізнес-логіки (thermostat, defrost, lighting) **ніколи** не працюють з GPIO напряму. Вони запитують драйвери по ролі: "дай мені compressor" → отримують IActuatorDriver.

---

## Структура проекту після Phase 4

```
ModESP_v4/
│
├── components/
│   ├── etl/                              # ETL бібліотека (git submodule)
│   │
│   ├── modesp_core/                      # ═══ ЯДРО (без змін з Phase 1-3) ═══
│   │   ├── include/modesp/
│   │   │   ├── core.h
│   │   │   ├── types.h
│   │   │   ├── message_types.h
│   │   │   ├── base_module.h
│   │   │   ├── module_manager.h
│   │   │   ├── shared_state.h
│   │   │   └── app.h
│   │   └── src/
│   │
│   ├── modesp_services/                  # ═══ СИСТЕМНІ СЕРВІСИ ═══
│   │   ├── include/modesp/services/
│   │   │   ├── error_service.h           # Phase 2 (без змін)
│   │   │   ├── watchdog_service.h        # Phase 2 (без змін)
│   │   │   ├── logger_service.h          # Phase 2 (без змін)
│   │   │   ├── system_monitor.h          # Phase 2 (без змін)
│   │   │   └── config_service.h          # ★ НОВИЙ — парсинг JSON конфігурації
│   │   └── src/
│   │       └── config_service.cpp        # ★ НОВИЙ
│   │
│   ├── modesp_hal/                       # ★ НОВИЙ КОМПОНЕНТ ★
│   │   ├── CMakeLists.txt
│   │   ├── include/modesp/hal/
│   │   │   ├── hal.h                     # HAL клас — фабрика ресурсів
│   │   │   ├── hal_types.h               # Типи ресурсів (GpioResource, etc.)
│   │   │   ├── driver_interfaces.h       # ISensorDriver, IActuatorDriver
│   │   │   └── driver_manager.h          # DriverManager — створює драйвери
│   │   └── src/
│   │       ├── hal.cpp
│   │       └── driver_manager.cpp
│   │
│   ├── drivers/                          # ═══ ДРАЙВЕРИ (рефакторинг) ═══
│   │   ├── ds18b20/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── manifest.json             # ★ НОВИЙ — опис драйвера
│   │   │   ├── include/ds18b20_driver.h  # Рефакторинг → ISensorDriver
│   │   │   └── src/ds18b20_driver.cpp
│   │   │
│   │   ├── relay/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── manifest.json             # ★ НОВИЙ
│   │   │   ├── include/relay_driver.h    # Рефакторинг → IActuatorDriver
│   │   │   └── src/relay_driver.cpp
│   │   │
│   │   └── pwm/                          # ★ НОВИЙ ДРАЙВЕР (заготовка)
│   │       ├── CMakeLists.txt
│   │       ├── manifest.json
│   │       ├── include/pwm_driver.h
│   │       └── src/pwm_driver.cpp
│   │
│   └── modules/                          # ═══ БІЗНЕС-ЛОГІКА (рефакторинг) ═══
│       ├── thermostat/
│       │   ├── CMakeLists.txt
│       │   ├── manifest.json             # ★ НОВИЙ — опис модуля
│       │   ├── include/thermostat_module.h  # Рефакторинг → використовує IActuatorDriver
│       │   └── src/thermostat_module.cpp
│       │
│       └── led_blink/                    # Залишається як є (системний)
│           └── ...
│
├── data/                                 # ★ НОВИЙ — LittleFS partition ★
│   ├── board.json                        # Опис hardware плати
│   └── bindings.json                     # Зв'язки hardware↔функція
│
├── main/
│   ├── CMakeLists.txt
│   └── main.cpp                          # ★ РЕФАКТОРИНГ — новий boot flow
│
├── partitions.csv                        # ★ ОНОВЛЕНИЙ — додано data partition
├── CMakeLists.txt
└── sdkconfig
```

---

## Системні ліміти (constexpr)

Визначаються в `modesp/hal/hal_types.h`. Фіксують максимальні розміри статичних пулів.

```cpp
// modesp/hal/hal_types.h
#pragma once
#include "etl/string.h"
#include "etl/vector.h"
#include "driver/gpio.h"

namespace modesp {

// ═══ Системні ліміти ═══
static constexpr size_t MAX_RELAYS         = 8;
static constexpr size_t MAX_ONEWIRE_BUSES  = 4;
static constexpr size_t MAX_ADC_CHANNELS   = 8;
static constexpr size_t MAX_PWM_CHANNELS   = 8;
static constexpr size_t MAX_I2C_BUSES      = 2;
static constexpr size_t MAX_BINDINGS       = 24;
static constexpr size_t MAX_SENSORS        = 16;
static constexpr size_t MAX_ACTUATORS      = 16;

// ═══ Типи ідентифікаторів ═══
using HalId   = etl::string<16>;   // "relay_1", "ow_1"
using Role    = etl::string<16>;   // "compressor", "chamber_temp"
using DriverType = etl::string<16>; // "relay", "ds18b20", "pwm"
using ModuleName = etl::string<16>; // "thermostat", "lighting"

// ═══ Конфігурація ресурсів (результат парсингу board.json) ═══

struct GpioOutputConfig {
    HalId id;                // "relay_1"
    gpio_num_t gpio;         // GPIO_NUM_16
    bool active_high;        // true = HIGH = ON
};

struct OneWireBusConfig {
    HalId id;                // "ow_1"
    gpio_num_t gpio;         // GPIO_NUM_4
};

struct AdcChannelConfig {
    HalId id;                // "adc_1"
    gpio_num_t gpio;         // GPIO_NUM_34
    uint8_t atten;           // ADC_ATTEN_DB_11
};

struct PwmChannelConfig {
    HalId id;                // "pwm_1"
    gpio_num_t gpio;         // GPIO_NUM_25
    uint32_t frequency;      // 25000 Hz
};

struct I2cBusConfig {
    HalId id;                // "i2c_0"
    gpio_num_t sda;          // GPIO_NUM_21
    gpio_num_t scl;          // GPIO_NUM_22
    uint32_t speed_hz;       // 100000
};

// ═══ Конфігурація плати (повний результат парсингу board.json) ═══

struct BoardConfig {
    etl::string<24> board_name;                                // "cold_room_v1"
    etl::string<8>  board_version;                             // "1.0"
    etl::vector<GpioOutputConfig, MAX_RELAYS> relays;
    etl::vector<OneWireBusConfig, MAX_ONEWIRE_BUSES> onewire_buses;
    etl::vector<AdcChannelConfig, MAX_ADC_CHANNELS> adc_channels;
    etl::vector<PwmChannelConfig, MAX_PWM_CHANNELS> pwm_channels;
    etl::vector<I2cBusConfig, MAX_I2C_BUSES> i2c_buses;
};

// ═══ Binding (результат парсингу bindings.json) ═══

struct Binding {
    HalId      hardware_id;   // "relay_1"    — посилання на board.json
    Role       role;          // "compressor" — логічна роль
    DriverType driver_type;   // "relay"      — тип драйвера
    ModuleName module_name;   // "thermostat" — який модуль використовує
};

struct BindingTable {
    etl::vector<Binding, MAX_BINDINGS> bindings;
};

// ═══ Готові ресурси HAL (після ініціалізації GPIO) ═══

struct GpioOutputResource {
    HalId id;
    gpio_num_t gpio;
    bool active_high;
    bool initialized = false;
};

struct OneWireBusResource {
    HalId id;
    gpio_num_t gpio;
    bool initialized = false;
};

struct AdcChannelResource {
    HalId id;
    gpio_num_t gpio;
    uint8_t atten;
    bool initialized = false;
};

struct PwmChannelResource {
    HalId id;
    gpio_num_t gpio;
    uint32_t frequency;
    uint8_t ledc_channel;    // Автоматично призначається HAL
    bool initialized = false;
};

} // namespace modesp
```

---

## ConfigService — парсинг JSON конфігурації

ConfigService читає JSON файли з LittleFS при boot. Парсинг виконується на стеку за допомогою jsmn (zero heap allocation парсер, ~400 байт коду).

```cpp
// modesp/services/config_service.h
#pragma once
#include "modesp/base_module.h"
#include "modesp/hal/hal_types.h"

namespace modesp {

class ConfigService : public BaseModule {
public:
    ConfigService()
        : BaseModule("config", ModulePriority::CRITICAL) {}

    bool on_init() override;

    // ─── Доступ до результатів парсингу ───
    const BoardConfig&  board_config()  const { return board_config_; }
    const BindingTable& binding_table() const { return binding_table_; }

    // ─── Runtime оновлення bindings (з WebUI) ───
    bool save_bindings(const BindingTable& new_bindings);

private:
    BoardConfig  board_config_;
    BindingTable binding_table_;

    // ─── Внутрішні методи парсингу ───
    bool mount_littlefs();
    bool parse_board_json();
    bool parse_bindings_json();

    // jsmn helper: парсить файл у стековий буфер
    // max_file_size — максимальний розмір файлу
    // max_tokens — максимальна кількість JSON токенів
    static constexpr size_t MAX_FILE_SIZE = 2048;
    static constexpr size_t MAX_TOKENS    = 128;
};

} // namespace modesp
```

### Імплементація парсингу

```cpp
// config_service.cpp (ключові фрагменти)

bool ConfigService::on_init() {
    if (!mount_littlefs()) {
        ESP_LOGE(TAG, "Failed to mount LittleFS");
        return false;
    }

    if (!parse_board_json()) {
        ESP_LOGE(TAG, "Failed to parse board.json");
        return false;
    }
    ESP_LOGI(TAG, "Board: %s v%s (%d relays, %d ow_buses)",
             board_config_.board_name.c_str(),
             board_config_.board_version.c_str(),
             board_config_.relays.size(),
             board_config_.onewire_buses.size());

    if (!parse_bindings_json()) {
        ESP_LOGE(TAG, "Failed to parse bindings.json");
        return false;
    }
    ESP_LOGI(TAG, "Loaded %d bindings", binding_table_.bindings.size());

    return true;
}

bool ConfigService::mount_littlefs() {
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/data",
        .partition_label = "data",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    esp_err_t err = esp_vfs_littlefs_register(&conf);
    return (err == ESP_OK);
}

bool ConfigService::parse_board_json() {
    // 1. Читаємо файл у стековий буфер
    char buf[MAX_FILE_SIZE];
    FILE* f = fopen("/data/board.json", "r");
    if (!f) return false;
    size_t len = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[len] = '\0';

    // 2. Парсимо jsmn на стеку
    jsmn_parser parser;
    jsmntok_t tokens[MAX_TOKENS];
    jsmn_init(&parser);
    int token_count = jsmn_parse(&parser, buf, len, tokens, MAX_TOKENS);
    if (token_count < 0) return false;

    // 3. Обходимо токени, заповнюємо BoardConfig
    // (парсинг relays, onewire_buses, adc_channels, pwm_channels)
    // ...
    return true;
}
```

### JSON файли

**data/board.json** — опис фізичної плати:
```json
{
  "board": "cold_room_v1",
  "version": "1.0",
  "relays": [
    {"id": "relay_1", "gpio": 16, "active_high": true},
    {"id": "relay_2", "gpio": 17, "active_high": true},
    {"id": "relay_3", "gpio": 18, "active_high": false},
    {"id": "relay_4", "gpio": 19, "active_high": true},
    {"id": "relay_5", "gpio": 21, "active_high": true},
    {"id": "relay_6", "gpio": 22, "active_high": true}
  ],
  "onewire_buses": [
    {"id": "ow_1", "gpio": 4}
  ],
  "adc_channels": [
    {"id": "adc_1", "gpio": 34, "atten": 11},
    {"id": "adc_2", "gpio": 35, "atten": 11}
  ],
  "pwm_channels": [
    {"id": "pwm_1", "gpio": 25, "frequency": 25000}
  ]
}
```

**data/bindings.json** — зв'язки hardware↔функція (змінюється оператором через WebUI):
```json
{
  "bindings": [
    {"hardware": "relay_1", "role": "compressor",   "driver": "relay",   "module": "thermostat"},
    {"hardware": "relay_2", "role": "fan",           "driver": "relay",   "module": "ventilation"},
    {"hardware": "relay_3", "role": "light",         "driver": "relay",   "module": "lighting"},
    {"hardware": "relay_4", "role": "defrost_heater","driver": "relay",   "module": "defrost"},
    {"hardware": "relay_5", "role": "door_lock",     "driver": "relay",   "module": "access"},
    {"hardware": "relay_6", "role": "alarm_siren",   "driver": "relay",   "module": "alarm"},
    {"hardware": "ow_1",    "role": "chamber_temp",  "driver": "ds18b20", "module": "thermostat"},
    {"hardware": "pwm_1",   "role": "evap_fan",      "driver": "pwm",     "module": "ventilation"}
  ]
}
```

---

## HAL — Hardware Abstraction Layer

HAL отримує BoardConfig від ConfigService і ініціалізує фізичні ресурси. Він не знає про ролі чи модулі — тільки про hardware id та GPIO.

```cpp
// modesp/hal/hal.h
#pragma once
#include "modesp/hal/hal_types.h"

namespace modesp {

class HAL {
public:
    // Ініціалізація з конфігурації плати
    bool init(const BoardConfig& config);

    // ─── Пошук ресурсів по hal_id ───
    GpioOutputResource*  find_gpio_output(etl::string_view id);
    OneWireBusResource*  find_onewire_bus(etl::string_view id);
    AdcChannelResource*  find_adc_channel(etl::string_view id);
    PwmChannelResource*  find_pwm_channel(etl::string_view id);

    // ─── Статистика ───
    size_t relay_count()   const { return relay_count_; }
    size_t onewire_count() const { return onewire_count_; }
    size_t adc_count()     const { return adc_count_; }
    size_t pwm_count()     const { return pwm_count_; }

private:
    // Статичні пули ресурсів — zero heap
    etl::array<GpioOutputResource, MAX_RELAYS>        relays_;
    etl::array<OneWireBusResource, MAX_ONEWIRE_BUSES> onewire_buses_;
    etl::array<AdcChannelResource, MAX_ADC_CHANNELS>  adc_channels_;
    etl::array<PwmChannelResource, MAX_PWM_CHANNELS>  pwm_channels_;

    size_t relay_count_   = 0;
    size_t onewire_count_ = 0;
    size_t adc_count_     = 0;
    size_t pwm_count_     = 0;

    // ─── Ініціалізація конкретних типів ───
    bool init_relays(const BoardConfig& config);
    bool init_onewire(const BoardConfig& config);
    bool init_adc(const BoardConfig& config);
    bool init_pwm(const BoardConfig& config);
};

} // namespace modesp
```

### HAL імплементація

```cpp
// hal.cpp (ключові фрагменти)

bool HAL::init(const BoardConfig& config) {
    ESP_LOGI(TAG, "Initializing HAL for board '%s'", config.board_name.c_str());

    if (!init_relays(config))  return false;
    if (!init_onewire(config)) return false;
    if (!init_adc(config))     return false;
    if (!init_pwm(config))     return false;

    ESP_LOGI(TAG, "HAL ready: %d relays, %d ow, %d adc, %d pwm",
             relay_count_, onewire_count_, adc_count_, pwm_count_);
    return true;
}

bool HAL::init_relays(const BoardConfig& config) {
    for (const auto& cfg : config.relays) {
        if (relay_count_ >= MAX_RELAYS) {
            ESP_LOGW(TAG, "Max relays reached (%d)", MAX_RELAYS);
            break;
        }
        // Конфігуруємо GPIO
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << cfg.gpio);
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        if (gpio_config(&io_conf) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init GPIO %d for %s", cfg.gpio, cfg.id.c_str());
            continue;
        }
        // Вимикаємо при старті (безпечний стан)
        gpio_set_level(cfg.gpio, cfg.active_high ? 0 : 1);

        auto& res = relays_[relay_count_++];
        res.id = cfg.id;
        res.gpio = cfg.gpio;
        res.active_high = cfg.active_high;
        res.initialized = true;

        ESP_LOGI(TAG, "  Relay '%s' on GPIO %d (active_%s)",
                 cfg.id.c_str(), cfg.gpio, cfg.active_high ? "HIGH" : "LOW");
    }
    return true;
}

GpioOutputResource* HAL::find_gpio_output(etl::string_view id) {
    for (size_t i = 0; i < relay_count_; i++) {
        if (relays_[i].id == id) return &relays_[i];
    }
    return nullptr;
}

// Аналогічно для find_onewire_bus, find_adc_channel, find_pwm_channel
```

---

## Інтерфейси драйверів

```cpp
// modesp/hal/driver_interfaces.h
#pragma once
#include "modesp/hal/hal_types.h"

namespace modesp {

// ═══════════════════════════════════════════
//  ISensorDriver — інтерфейс для всіх датчиків
// ═══════════════════════════════════════════

class ISensorDriver {
public:
    virtual ~ISensorDriver() = default;

    // Lifecycle
    virtual bool init() = 0;
    virtual void update(uint32_t dt_ms) = 0;

    // Зчитування
    virtual bool read(float& value) = 0;     // true = валідне читання
    virtual bool is_healthy() const = 0;

    // Ідентифікація
    virtual const char* role() const = 0;     // "chamber_temp"
    virtual const char* type() const = 0;     // "ds18b20"

    // Діагностика
    virtual uint32_t error_count() const { return 0; }
    virtual uint32_t read_count() const { return 0; }
};

// ═══════════════════════════════════════════
//  IActuatorDriver — інтерфейс для актуаторів
// ═══════════════════════════════════════════

class IActuatorDriver {
public:
    virtual ~IActuatorDriver() = default;

    // Lifecycle
    virtual bool init() = 0;
    virtual void update(uint32_t dt_ms) = 0;

    // Дискретне керування (relay)
    virtual bool set(bool state) = 0;
    virtual bool get_state() const = 0;

    // Аналогове керування (PWM) — дефолтна реалізація через set()
    virtual bool set_value(float value_0_1) {
        return set(value_0_1 > 0.5f);
    }
    virtual float get_value() const {
        return get_state() ? 1.0f : 0.0f;
    }

    // Можливості
    virtual bool supports_analog() const { return false; }

    // Ідентифікація
    virtual const char* role() const = 0;     // "compressor"
    virtual const char* type() const = 0;     // "relay"

    // Безпека
    virtual bool is_healthy() const = 0;
    virtual void emergency_stop() { set(false); }

    // Діагностика
    virtual uint32_t switch_count() const { return 0; }
    virtual uint32_t total_on_time_s() const { return 0; }
};

} // namespace modesp
```

---

## DriverManager — створення та зв'язування драйверів

DriverManager читає BindingTable і для кожного binding:
1. Знаходить hardware ресурс в HAL
2. Створює драйвер потрібного типу зі статичного пулу
3. Передає йому ресурс та роль

```cpp
// modesp/hal/driver_manager.h
#pragma once
#include "modesp/hal/hal.h"
#include "modesp/hal/driver_interfaces.h"
#include "modesp/hal/hal_types.h"

// Включаємо конкретні драйвери
#include "ds18b20_driver.h"
#include "relay_driver.h"
// #include "pwm_driver.h"  // Phase 4+

namespace modesp {

class DriverManager {
public:
    // Створити всі драйвери згідно bindings
    bool init(const BindingTable& bindings, HAL& hal);

    // ─── Пошук драйверів по ролі ───
    ISensorDriver*   find_sensor(etl::string_view role);
    IActuatorDriver* find_actuator(etl::string_view role);

    // ─── Пошук всіх драйверів для модуля ───
    // Повертає кількість знайдених
    size_t find_sensors_for_module(etl::string_view module_name,
                                   ISensorDriver* out[], size_t max_count);
    size_t find_actuators_for_module(etl::string_view module_name,
                                     IActuatorDriver* out[], size_t max_count);

    // ─── Update всіх драйверів (викликається з main loop) ───
    void update_all(uint32_t dt_ms);

    // ─── Статистика ───
    size_t sensor_count()   const { return sensor_count_; }
    size_t actuator_count() const { return actuator_count_; }

private:
    // ═══ Статичні пули драйверів ═══
    // Кожен тип драйвера має свій пул фіксованого розміру.
    // Драйвери створюються in-place, zero heap.

    etl::array<DS18B20Driver, MAX_SENSORS>  ds18b20_pool_;
    etl::array<RelayDriver, MAX_ACTUATORS>  relay_pool_;
    // etl::array<PwmDriver, MAX_PWM_CHANNELS>  pwm_pool_;

    size_t ds18b20_count_ = 0;
    size_t relay_count_   = 0;
    // size_t pwm_count_   = 0;

    // ═══ Уніфіковані масиви вказівників на інтерфейси ═══
    struct SensorEntry {
        ISensorDriver* driver;
        Role role;
        ModuleName module;
    };
    struct ActuatorEntry {
        IActuatorDriver* driver;
        Role role;
        ModuleName module;
    };

    etl::vector<SensorEntry, MAX_SENSORS>     sensors_;
    etl::vector<ActuatorEntry, MAX_ACTUATORS> actuators_;

    size_t sensor_count_   = 0;
    size_t actuator_count_ = 0;

    // ─── Створення конкретного драйвера ───
    ISensorDriver*   create_sensor(const Binding& binding, HAL& hal);
    IActuatorDriver* create_actuator(const Binding& binding, HAL& hal);
};

} // namespace modesp
```

### DriverManager імплементація

```cpp
// driver_manager.cpp (ключові фрагменти)

bool DriverManager::init(const BindingTable& bindings, HAL& hal) {
    ESP_LOGI(TAG, "Creating drivers for %d bindings...", bindings.bindings.size());

    for (const auto& binding : bindings.bindings) {
        // Визначаємо тип: sensor чи actuator
        if (binding.driver_type == "ds18b20" ||
            binding.driver_type == "ntc" ||
            binding.driver_type == "gpio_input")
        {
            auto* driver = create_sensor(binding, hal);
            if (driver) {
                sensors_.push_back({driver, binding.role, binding.module_name});
                ESP_LOGI(TAG, "  Sensor '%s' [%s] → module '%s'",
                         binding.role.c_str(), binding.driver_type.c_str(),
                         binding.module_name.c_str());
            }
        }
        else if (binding.driver_type == "relay" ||
                 binding.driver_type == "pwm" ||
                 binding.driver_type == "gpio_output")
        {
            auto* driver = create_actuator(binding, hal);
            if (driver) {
                actuators_.push_back({driver, binding.role, binding.module_name});
                ESP_LOGI(TAG, "  Actuator '%s' [%s] → module '%s'",
                         binding.role.c_str(), binding.driver_type.c_str(),
                         binding.module_name.c_str());
            }
        }
        else {
            ESP_LOGW(TAG, "  Unknown driver type: '%s'", binding.driver_type.c_str());
        }
    }

    // Ініціалізуємо всі створені драйвери
    for (auto& entry : sensors_) {
        if (!entry.driver->init()) {
            ESP_LOGE(TAG, "Failed to init sensor '%s'", entry.role.c_str());
        }
    }
    for (auto& entry : actuators_) {
        if (!entry.driver->init()) {
            ESP_LOGE(TAG, "Failed to init actuator '%s'", entry.role.c_str());
        }
    }

    sensor_count_ = sensors_.size();
    actuator_count_ = actuators_.size();
    ESP_LOGI(TAG, "DriverManager ready: %d sensors, %d actuators",
             sensor_count_, actuator_count_);
    return true;
}

ISensorDriver* DriverManager::create_sensor(const Binding& binding, HAL& hal) {
    if (binding.driver_type == "ds18b20") {
        auto* bus = hal.find_onewire_bus(binding.hardware_id);
        if (!bus) {
            ESP_LOGE(TAG, "OneWire bus '%s' not found in HAL", binding.hardware_id.c_str());
            return nullptr;
        }
        if (ds18b20_count_ >= MAX_SENSORS) {
            ESP_LOGE(TAG, "DS18B20 pool full");
            return nullptr;
        }
        auto& driver = ds18b20_pool_[ds18b20_count_++];
        driver.configure(binding.role.c_str(), bus->gpio);
        return &driver;
    }
    // Інші типи датчиків додаються тут
    return nullptr;
}

IActuatorDriver* DriverManager::create_actuator(const Binding& binding, HAL& hal) {
    if (binding.driver_type == "relay") {
        auto* res = hal.find_gpio_output(binding.hardware_id);
        if (!res) {
            ESP_LOGE(TAG, "GPIO output '%s' not found in HAL", binding.hardware_id.c_str());
            return nullptr;
        }
        if (relay_count_ >= MAX_ACTUATORS) {
            ESP_LOGE(TAG, "Relay pool full");
            return nullptr;
        }
        auto& driver = relay_pool_[relay_count_++];
        driver.configure(binding.role.c_str(), res->gpio, res->active_high);
        return &driver;
    }
    // PWM, GPIO output додаються тут
    return nullptr;
}

ISensorDriver* DriverManager::find_sensor(etl::string_view role) {
    for (auto& entry : sensors_) {
        if (entry.role == role) return entry.driver;
    }
    return nullptr;
}

IActuatorDriver* DriverManager::find_actuator(etl::string_view role) {
    for (auto& entry : actuators_) {
        if (entry.role == role) return entry.driver;
    }
    return nullptr;
}

size_t DriverManager::find_sensors_for_module(etl::string_view module_name,
                                               ISensorDriver* out[], size_t max_count) {
    size_t count = 0;
    for (auto& entry : sensors_) {
        if (entry.module == module_name && count < max_count) {
            out[count++] = entry.driver;
        }
    }
    return count;
}

void DriverManager::update_all(uint32_t dt_ms) {
    for (auto& entry : sensors_)   entry.driver->update(dt_ms);
    for (auto& entry : actuators_) entry.driver->update(dt_ms);
}
```

---

## Рефакторинг драйверів

### DS18B20Driver → ISensorDriver

Поточний DS18B20Driver наслідує BaseModule. Рефакторимо його як ISensorDriver.
Вся OneWire логіка, CRC8, валідація, rate-of-change — залишаються.

```cpp
// drivers/ds18b20/include/ds18b20_driver.h
#pragma once
#include "modesp/hal/driver_interfaces.h"
#include "driver/gpio.h"

namespace modesp {

class DS18B20Driver : public ISensorDriver {
public:
    DS18B20Driver() = default;  // Для розміщення в пулі

    // Конфігурація (замість конструктора, бо пул)
    void configure(const char* role, gpio_num_t gpio,
                   uint32_t read_interval_ms = 1000);

    // ─── ISensorDriver interface ───
    bool init() override;
    void update(uint32_t dt_ms) override;
    bool read(float& value) override;
    bool is_healthy() const override;
    const char* role() const override { return role_; }
    const char* type() const override { return "ds18b20"; }
    uint32_t error_count() const override { return consecutive_errors_; }

private:
    char role_[17] = {};           // "chamber_temp"
    gpio_num_t gpio_ = GPIO_NUM_NC;
    uint32_t read_interval_ms_ = 1000;

    // Стан
    float last_valid_temp_ = 0.0f;
    bool has_valid_reading_ = false;
    uint32_t since_last_read_ = 0;
    uint32_t consecutive_errors_ = 0;
    int64_t last_read_time_us_ = 0;

    // Валідація
    static constexpr float MIN_VALID_TEMP = -55.0f;
    static constexpr float MAX_VALID_TEMP = 125.0f;
    static constexpr float MAX_RATE_PER_SEC = 5.0f;
    static constexpr uint32_t MAX_CONSECUTIVE_ERRORS = 10;

    // OneWire primitives (переносяться з поточної реалізації)
    bool reset_bus();
    void write_byte(uint8_t data);
    uint8_t read_byte();
    bool start_conversion();
    bool read_scratchpad(uint8_t* buf, size_t len);
    bool validate_reading(float temp);
    static uint8_t crc8(const uint8_t* data, size_t len);
};

} // namespace modesp
```

### RelayDriver → IActuatorDriver

```cpp
// drivers/relay/include/relay_driver.h
#pragma once
#include "modesp/hal/driver_interfaces.h"
#include "driver/gpio.h"

namespace modesp {

class RelayDriver : public IActuatorDriver {
public:
    RelayDriver() = default;  // Для розміщення в пулі

    // Конфігурація
    void configure(const char* role, gpio_num_t gpio, bool active_high,
                   uint32_t min_switch_ms = 0);

    // ─── IActuatorDriver interface ───
    bool init() override;
    void update(uint32_t dt_ms) override;
    bool set(bool state) override;
    bool get_state() const override { return current_state_; }
    bool is_healthy() const override { return initialized_; }
    const char* role() const override { return role_; }
    const char* type() const override { return "relay"; }
    void emergency_stop() override;
    uint32_t switch_count() const override { return switch_count_; }
    uint32_t total_on_time_s() const override { return total_on_ms_ / 1000; }

private:
    char role_[17] = {};
    gpio_num_t gpio_ = GPIO_NUM_NC;
    bool active_high_ = true;
    uint32_t min_switch_ms_ = 0;      // Захист компресора

    // Стан
    bool current_state_ = false;
    bool initialized_ = false;
    uint32_t since_last_switch_ = 0;
    uint32_t switch_count_ = 0;
    uint32_t total_on_ms_ = 0;

    void apply_gpio(bool state);
};

} // namespace modesp
```

---

## Рефакторинг ThermostatModule

Thermostat більше не знає про GPIO, OneWire, або конкретні драйвери. Він працює тільки через інтерфейси.

```cpp
// modules/thermostat/include/thermostat_module.h
#pragma once
#include "modesp/base_module.h"
#include "modesp/hal/driver_interfaces.h"

namespace modesp {

class DriverManager;  // forward declaration

class ThermostatModule : public BaseModule {
public:
    ThermostatModule()
        : BaseModule("thermostat", ModulePriority::NORMAL) {}

    // DriverManager передає посилання при створенні
    void bind_drivers(DriverManager& dm);

    bool on_init() override;
    void on_update(uint32_t dt_ms) override;

private:
    // Драйвери (отримані від DriverManager)
    ISensorDriver*   temp_sensor_ = nullptr;
    IActuatorDriver* compressor_  = nullptr;

    // Параметри (читаються з NVS при init)
    float setpoint_    = -18.0f;
    float hysteresis_  = 1.5f;
    uint32_t min_off_ms_ = 180000;

    // Стан
    bool compressor_on_ = false;
    uint32_t off_timer_  = 0;
};

} // namespace modesp
```

### ThermostatModule імплементація

```cpp
// thermostat_module.cpp

void ThermostatModule::bind_drivers(DriverManager& dm) {
    temp_sensor_ = dm.find_sensor("chamber_temp");
    compressor_  = dm.find_actuator("compressor");
}

bool ThermostatModule::on_init() {
    if (!temp_sensor_) {
        ESP_LOGE(TAG, "No sensor with role 'chamber_temp' bound");
        return false;
    }
    if (!compressor_) {
        ESP_LOGE(TAG, "No actuator with role 'compressor' bound");
        return false;
    }

    // Читаємо settings з NVS
    // setpoint_ = nvs_get_float("thermo.setpoint", -18.0f);
    // hysteresis_ = nvs_get_float("thermo.hysteresis", 1.5f);

    ESP_LOGI(TAG, "Thermostat initialized (setpoint=%.1f°C, hyst=%.1f°C)",
             setpoint_, hysteresis_);
    ESP_LOGI(TAG, "  sensor: %s [%s]", temp_sensor_->role(), temp_sensor_->type());
    ESP_LOGI(TAG, "  actuator: %s [%s]", compressor_->role(), compressor_->type());
    return true;
}

void ThermostatModule::on_update(uint32_t dt_ms) {
    // Оновлюємо таймер відключення
    if (!compressor_on_) {
        off_timer_ += dt_ms;
    }

    // Читаємо температуру
    float temp;
    if (!temp_sensor_->read(temp)) {
        return;  // Немає валідного читання — тримаємо поточний стан
    }

    // Публікуємо в SharedState
    state_set_float("thermostat.temperature", temp);

    // Логіка on/off з гістерезисом
    if (!compressor_on_ && temp > setpoint_ + hysteresis_) {
        // Перевіряємо захист компресора
        if (off_timer_ >= min_off_ms_) {
            compressor_->set(true);
            compressor_on_ = true;
            ESP_LOGI(TAG, "Compressor ON (temp=%.1f°C > %.1f°C)",
                     temp, setpoint_ + hysteresis_);
            state_set_bool("thermostat.compressor", true);
        }
    }
    else if (compressor_on_ && temp < setpoint_ - hysteresis_) {
        compressor_->set(false);
        compressor_on_ = false;
        off_timer_ = 0;
        ESP_LOGI(TAG, "Compressor OFF (temp=%.1f°C < %.1f°C)",
                 temp, setpoint_ - hysteresis_);
        state_set_bool("thermostat.compressor", false);
    }
}
```

---

## Boot Flow — повна послідовність

```cpp
// main/main.cpp

#include "modesp/app.h"
#include "modesp/services/config_service.h"
#include "modesp/services/error_service.h"
#include "modesp/services/watchdog_service.h"
#include "modesp/services/logger_service.h"
#include "modesp/services/system_monitor.h"
#include "modesp/hal/hal.h"
#include "modesp/hal/driver_manager.h"
#include "thermostat_module.h"
// Інші модулі бізнес-логіки...

using namespace modesp;

// ═══ Статичні екземпляри (zero heap) ═══

// Системні сервіси
static ConfigService    config_service;
static ErrorService     error_service;
static WatchdogService  watchdog_service;
static LoggerService    logger_service;
static SystemMonitor    system_monitor;

// HAL та драйвери
static HAL              hal;
static DriverManager    driver_manager;

// Модулі бізнес-логіки (всі потенційні модулі скомпільовані)
static ThermostatModule thermostat;
// static VentilationModule ventilation;
// static LightingModule    lighting;
// static DefrostModule     defrost;
// static AccessModule      access;
// static AlarmModule       alarm;

extern "C" void app_main() {
    auto& app = App::instance();

    ESP_LOGI(TAG, "╔══════════════════════════════════╗");
    ESP_LOGI(TAG, "║   ModESP v4.0.0 — Phase 4       ║");
    ESP_LOGI(TAG, "║   HAL + Dynamic Configuration    ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════╝");

    // ═══ STAGE 1: Core init ═══
    app.init();

    // ═══ STAGE 2: Системні сервіси ═══
    app.modules().register_module(error_service);
    app.modules().register_module(watchdog_service);
    app.modules().register_module(config_service);  // Парсить JSON
    app.modules().register_module(logger_service);
    app.modules().register_module(system_monitor);

    // ═══ STAGE 3: HAL — ініціалізація hardware ═══
    // ConfigService має бути ініціалізований (on_init() вже пройшов)
    // Тому HAL ініціалізується ПІСЛЯ init_all() системних сервісів
    // АБО ConfigService.on_init() запускається першим через CRITICAL пріоритет

    // ═══ STAGE 4: Після init системних сервісів — будуємо HAL ═══
    // Це робиться після першого init_all() або через callback
    // Можна зробити явний двофазний init:

    // Фаза 1: init системних сервісів (включно з ConfigService)
    // config_service.on_init() → парсить board.json + bindings.json

    // Фаза 2: init HAL + drivers
    hal.init(config_service.board_config());
    driver_manager.init(config_service.binding_table(), hal);

    // ═══ STAGE 5: Активація модулів з bindings ═══
    // Перевіряємо які модулі потрібні
    auto& bindings = config_service.binding_table();

    // Для кожного унікального module_name в bindings — активуємо модуль
    auto has_module = [&](const char* name) -> bool {
        for (const auto& b : bindings.bindings) {
            if (b.module_name == name) return true;
        }
        return false;
    };

    if (has_module("thermostat")) {
        thermostat.bind_drivers(driver_manager);
        app.modules().register_module(thermostat);
        ESP_LOGI(TAG, "Activated module: thermostat");
    }
    // if (has_module("ventilation")) {
    //     ventilation.bind_drivers(driver_manager);
    //     app.modules().register_module(ventilation);
    // }
    // ... аналогічно для інших модулів

    ESP_LOGI(TAG, "Registered %d modules", app.modules().module_count());

    // ═══ STAGE 6: Запуск ═══
    // init_all() → start_all() → main loop
    // Драйвери оновлюються через driver_manager.update_all()
    // в main loop, перед modules.update_all()

    app.run();  // never returns
}
```

### Альтернативний boot flow (чистіший)

Замість явного HAL init в main, можна зробити це всередині App:

```cpp
// В App::init() або App::run():
// 1. register system services
// 2. init_all() system services → ConfigService парсить JSON
// 3. HAL::init(config_service.board_config())
// 4. DriverManager::init(config_service.binding_table(), hal)
// 5. Автоматична активація модулів з bindings
// 6. init_all() бізнес-модулів
// 7. start_all()
// 8. main loop: driver_manager.update_all() + modules.update_all()
```

---

## Main Loop з драйверами

```cpp
// Всередині App::run() або явно в main:

while (true) {
    uint32_t dt_ms = 10;  // 100 Hz

    // 1. Оновити всі драйвери (читання датчиків, таймери реле)
    driver_manager.update_all(dt_ms);

    // 2. Оновити всі модулі (бізнес-логіка)
    app.modules().update_all();

    // 3. Feed watchdog
    esp_task_wdt_reset();

    vTaskDelay(pdMS_TO_TICKS(dt_ms));
}
```

---

## Module Manifest (для Python генератора та WebUI)

Кожен модуль має manifest.json поруч з кодом. Він **не** парситься ESP — він для build tools і WebUI.

**modules/thermostat/manifest.json:**
```json
{
  "module": "thermostat",
  "version": "1.0.0",
  "description": "On/off thermostat with hysteresis and compressor protection",
  "requires": [
    {"role": "compressor",   "type": "actuator", "drivers": ["relay"]},
    {"role": "chamber_temp", "type": "sensor",   "drivers": ["ds18b20", "ntc"]}
  ],
  "provides": {
    "state_keys": [
      "thermostat.temperature",
      "thermostat.compressor",
      "thermostat.setpoint",
      "thermostat.mode"
    ],
    "commands": ["set_setpoint", "set_mode"]
  },
  "settings": [
    {"key": "setpoint",     "type": "float", "default": -18.0, "min": -40, "max": 10, "unit": "°C"},
    {"key": "hysteresis",   "type": "float", "default": 1.5,   "min": 0.5, "max": 5,  "unit": "°C"},
    {"key": "min_off_time", "type": "int",   "default": 180,   "min": 60,  "max": 600, "unit": "s"}
  ],
  "ui": {
    "icon": "thermometer",
    "color": "#2196F3",
    "widget": "thermostat_card"
  }
}
```

**drivers/ds18b20/manifest.json:**
```json
{
  "driver": "ds18b20",
  "version": "1.0.0",
  "description": "Dallas DS18B20 digital temperature sensor (OneWire)",
  "category": "sensor",
  "hardware_type": "onewire_bus",
  "output": {
    "type": "float",
    "unit": "°C",
    "range": [-55, 125],
    "resolution": 0.0625
  },
  "settings": [
    {"key": "read_interval", "type": "int",   "default": 1000, "min": 500, "max": 60000, "unit": "ms"},
    {"key": "offset",        "type": "float", "default": 0.0,  "min": -5.0, "max": 5.0, "unit": "°C"}
  ]
}
```

**drivers/relay/manifest.json:**
```json
{
  "driver": "relay",
  "version": "1.0.0",
  "description": "GPIO relay/SSR driver with min switch time protection",
  "category": "actuator",
  "hardware_type": "gpio_output",
  "supports_analog": false,
  "settings": [
    {"key": "min_switch_time", "type": "int", "default": 0, "min": 0, "max": 600, "unit": "s"}
  ]
}
```

---

## Partition Table

```csv
# Name,    Type, SubType,  Offset,   Size,   Flags
nvs,       data, nvs,      0x9000,   0x6000,
phy_init,  data, phy,      0xf000,   0x1000,
factory,   app,  factory,  0x10000,  0x180000,
data,      data, spiffs,   0x190000, 0x60000,
nvs_keys,  data, nvs_keys, 0x1f0000, 0x1000,
```

**Примітка:** SubType `spiffs` використовується і для LittleFS — ESP-IDF визначає FS по реєстрації, не по partition subtype.

Data partition: 384KB — достатньо для JSON конфігурації + WebUI файли в майбутньому.

---

## CMakeLists структура

### components/modesp_hal/CMakeLists.txt
```cmake
idf_component_register(
    SRCS "src/hal.cpp" "src/driver_manager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES modesp_core driver gpio ledc
    PRIV_REQUIRES ds18b20_driver relay_driver
)
```

### drivers/ds18b20/CMakeLists.txt
```cmake
idf_component_register(
    SRCS "src/ds18b20_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES modesp_hal
)
```

### drivers/relay/CMakeLists.txt
```cmake
idf_component_register(
    SRCS "src/relay_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES modesp_hal
)
```

### Залежності між компонентами Phase 4

```
main
 ├── modesp_core
 ├── modesp_services (config_service → LittleFS + jsmn)
 ├── modesp_hal
 │    ├── hal.cpp (GPIO, OneWire, ADC, PWM init)
 │    └── driver_manager.cpp
 │         ├── ds18b20_driver
 │         └── relay_driver
 └── modules/
      └── thermostat (uses ISensorDriver + IActuatorDriver)
```

---

## Очікуваний лог при boot

```
I main: ╔══════════════════════════════════╗
I main: ║   ModESP v4.0.0 — Phase 4       ║
I main: ║   HAL + Dynamic Configuration    ║
I main: ╚══════════════════════════════════╝
I App: Initializing ModESP v4.0.0
I App: Free heap: 284000 bytes
I ConfigService: Mounted LittleFS (used: 8KB / 384KB)
I ConfigService: Board: cold_room_v1 v1.0 (6 relays, 1 ow_buses)
I ConfigService: Loaded 8 bindings
I ErrorService: ErrorService initialized
I WatchdogSvc: WatchdogService initialized
I HAL: Initializing HAL for board 'cold_room_v1'
I HAL:   Relay 'relay_1' on GPIO 16 (active_HIGH)
I HAL:   Relay 'relay_2' on GPIO 17 (active_HIGH)
I HAL:   Relay 'relay_3' on GPIO 18 (active_LOW)
I HAL:   Relay 'relay_4' on GPIO 19 (active_HIGH)
I HAL:   Relay 'relay_5' on GPIO 21 (active_HIGH)
I HAL:   Relay 'relay_6' on GPIO 22 (active_HIGH)
I HAL:   OneWire 'ow_1' on GPIO 4
I HAL:   PWM 'pwm_1' on GPIO 25 (25000 Hz)
I HAL: HAL ready: 6 relays, 1 ow, 0 adc, 1 pwm
I DriverMgr: Creating drivers for 8 bindings...
I DriverMgr:   Actuator 'compressor' [relay] → module 'thermostat'
I DriverMgr:   Actuator 'fan' [relay] → module 'ventilation'
I DriverMgr:   Actuator 'light' [relay] → module 'lighting'
I DriverMgr:   Actuator 'defrost_heater' [relay] → module 'defrost'
I DriverMgr:   Actuator 'door_lock' [relay] → module 'access'
I DriverMgr:   Actuator 'alarm_siren' [relay] → module 'alarm'
I DriverMgr:   Sensor 'chamber_temp' [ds18b20] → module 'thermostat'
I DriverMgr:   Actuator 'evap_fan' [pwm] → module 'ventilation'
I DriverMgr: DriverManager ready: 1 sensors, 7 actuators
I main: Activated module: thermostat
I Thermostat: Thermostat initialized (setpoint=-18.0°C, hyst=1.5°C)
I Thermostat:   sensor: chamber_temp [ds18b20]
I Thermostat:   actuator: compressor [relay]
I SystemMonitor: Free heap: 280000 bytes
I App: Entering main loop (100 Hz)
I Thermostat: Compressor ON (temp=21.0°C > -16.5°C)
```

---

## План імплементації Phase 4

### Крок 1: Інфраструктура
1. Оновити `partitions.csv` — додати data partition
2. Створити `data/board.json` та `data/bindings.json`
3. Додати jsmn до проекту (один .h файл)
4. Налаштувати LittleFS в `sdkconfig`

### Крок 2: modesp_hal компонент
1. `hal_types.h` — всі типи та ліміти
2. `driver_interfaces.h` — ISensorDriver, IActuatorDriver
3. `hal.h/.cpp` — HAL клас з ресурсами
4. `driver_manager.h/.cpp` — створення та зв'язування драйверів

### Крок 3: ConfigService
1. `config_service.h/.cpp` — mount LittleFS + парсинг JSON

### Крок 4: Рефакторинг драйверів
1. DS18B20Driver: BaseModule → ISensorDriver
   - Зберегти всю OneWire логіку, CRC8, валідацію
   - Додати `configure()` замість конструктора з параметрами
   - Видалити наслідування від BaseModule
2. RelayDriver: BaseModule → IActuatorDriver
   - Зберегти min_switch_time логіку
   - Додати `configure()` замість конструктора
   - Видалити наслідування від BaseModule

### Крок 5: Рефакторинг ThermostatModule
1. Видалити пряме звернення до GPIO/OneWire
2. Додати `bind_drivers(DriverManager&)`
3. Працювати через ISensorDriver/IActuatorDriver

### Крок 6: Новий main.cpp
1. Двофазний boot: system services → HAL/drivers → business modules
2. Автоматична активація модулів з bindings

### Крок 7: Тестування
1. Build + flash
2. Перевірити boot log — всі стадії без помилок
3. Перевірити thermostat працює через інтерфейси
4. Перевірити heap залишається стабільним

---

## Що НЕ входить в Phase 4

- WiFi / WebUI / MQTT (Phase 5+)
- NVS для module settings (Phase 5)
- PWM driver повна реалізація (заготовка є, повний — при потребі)
- NTC driver, Pressure driver (додаються як нові драйвери пізніше)
- Python генератор для WebUI (Phase 6+)
- Module manifest парсинг на ESP (manifests тільки для build tools)
- Автоматичний discovery DS18B20 адрес (поки один датчик на шину)

---

*Документ підготовлено на основі обговорення архітектури ModESP v4 HAL, Phase 1-3 реалізації, та попередніх HAL reference документів.*
