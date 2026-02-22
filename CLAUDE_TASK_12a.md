# CLAUDE_TASK: Phase 12a — KC868-A6 Board Support (I2C Expander Drivers)

> Підтримка плати KinCony KC868-A6: реле та входи через I2C expander PCF8574.
> Прошивка залишається універсальною — board.json визначає яка плата.

---

## Контекст

KC868-A6 — промислова ESP32 плата для автоматизації:
- 6 реле через **PCF8574 I2C expander** (адреса 0x24) + ULN2003A
- 6 оптоізольованих digital inputs через **PCF8574** (адреса 0x22)
- 2 OneWire шини: GPIO 32, GPIO 33 (pull-up 4.7K на платі)
- 4 ADC входи 0-5V: GPIO 36, 39, 34, 35
- RS485 (TX=27, RX=14), RS232 (TX=17, RX=16)
- I2C шина: SDA=4, SCL=15 (+ RTC DS1307, + роз'єм OLED SSD1306)
- Живлення 12V DC

**Ключова різниця від dev board:** реле і входи НЕ на GPIO напряму, а через I2C expander.
Потрібні нові драйвери що реалізують ті самі інтерфейси IActuatorDriver/ISensorDriver.

**Бізнес-модулі (equipment, thermostat, defrost, protection) — БЕЗ ЗМІН.**
Вони працюють через find_actuator("compressor")->set(true) і їм байдуже
що всередині — GPIO чи I2C.

---

## Огляд архітектури

```
board.json (KC868-A6)     board.json (dev board)
        ↓                         ↓
   I2C expander config       GPIO output config
        ↓                         ↓
   HAL::init_i2c()           HAL::init_gpio_outputs()
        ↓                         ↓
   I2CExpanderResource       GpioOutputResource
        ↓                         ↓
   PCF8574RelayDriver        RelayDriver
        ↓                         ↓
   IActuatorDriver ←───── Equipment Module ─────→ IActuatorDriver
   (та сама абстракція)                          (та сама абстракція)
```

---

## Задача 1: I2C Bus та Expander типи в HAL

### hal_types.h — нові структури

Додати ПІСЛЯ існуючих struct (OneWireBusConfig, BoardConfig...):

```cpp
// ═══════════════════════════════════════════════════════════════
// I2C bus config (parsed from board.json)
// ═══════════════════════════════════════════════════════════════

struct I2CBusConfig {
    HalId id;              // "i2c_0"
    gpio_num_t sda;        // GPIO 4
    gpio_num_t scl;        // GPIO 15
    uint32_t freq_hz;      // 100000
};

struct I2CExpanderConfig {
    HalId id;              // "relay_exp"
    HalId bus_id;          // "i2c_0"
    HalId chip;            // "pcf8574"
    uint8_t address;       // 0x24
    uint8_t pin_count;     // 8
};

struct I2CExpanderOutputConfig {
    HalId id;              // "relay_1"
    HalId expander_id;     // "relay_exp"
    uint8_t pin;           // 0-7
    bool active_high;      // true
};

struct I2CExpanderInputConfig {
    HalId id;              // "din_1"
    HalId expander_id;     // "input_exp"
    uint8_t pin;           // 0-7
    bool invert;           // false
};
```

Додати в BoardConfig:
```cpp
struct BoardConfig {
    // ...existing...
    etl::vector<I2CBusConfig, 2>              i2c_buses;
    etl::vector<I2CExpanderConfig, 4>         i2c_expanders;
    etl::vector<I2CExpanderOutputConfig, 16>  expander_outputs;
    etl::vector<I2CExpanderInputConfig, 16>   expander_inputs;
};
```

Нові ліміти:
```cpp
static constexpr size_t MAX_I2C_BUSES      = 2;
static constexpr size_t MAX_I2C_EXPANDERS  = 4;
```

Нові resource structs:
```cpp
struct I2CBusResource {
    HalId id;
    i2c_port_t port;       // I2C_NUM_0 або I2C_NUM_1
    bool initialized = false;
};

struct I2CExpanderResource {
    HalId id;
    i2c_port_t i2c_port;
    uint8_t address;
    uint8_t pin_count;
    uint8_t output_state;  // Поточний стан всіх 8 виходів (бітова маска)
    bool initialized = false;

    /// Записати поточний output_state в PCF8574
    bool write_state();
    /// Прочитати всі входи з PCF8574
    bool read_state(uint8_t& input_byte);
};
```

---

## Задача 2: HAL — ініціалізація I2C

### hal.h — нові методи і масиви

```cpp
class HAL {
public:
    // ...existing...
    
    /// Find I2C expander resource by ID
    I2CExpanderResource* find_i2c_expander(etl::string_view id);

    /// Find expander output config by hardware ID
    I2CExpanderOutputConfig* find_expander_output(etl::string_view id);

    /// Find expander input config by hardware ID
    I2CExpanderInputConfig* find_expander_input(etl::string_view id);

private:
    // ...existing...
    
    etl::array<I2CBusResource, MAX_I2C_BUSES>          i2c_buses_;
    etl::array<I2CExpanderResource, MAX_I2C_EXPANDERS> i2c_expanders_;
    
    // Зберігаємо configs для lookup по hardware_id
    etl::vector<I2CExpanderOutputConfig, 16> expander_outputs_;
    etl::vector<I2CExpanderInputConfig, 16>  expander_inputs_;
    
    size_t i2c_bus_count_      = 0;
    size_t i2c_expander_count_ = 0;

    bool init_i2c(const BoardConfig& config);
    bool init_i2c_expanders(const BoardConfig& config);
};
```

### hal.cpp — I2C ініціалізація

```cpp
bool HAL::init_i2c(const BoardConfig& config) {
    i2c_bus_count_ = 0;
    
    for (const auto& cfg : config.i2c_buses) {
        if (i2c_bus_count_ >= MAX_I2C_BUSES) break;
        
        i2c_port_t port = (i2c_bus_count_ == 0) ? I2C_NUM_0 : I2C_NUM_1;
        
        i2c_config_t i2c_conf = {};
        i2c_conf.mode = I2C_MODE_MASTER;
        i2c_conf.sda_io_num = cfg.sda;
        i2c_conf.scl_io_num = cfg.scl;
        i2c_conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        i2c_conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        i2c_conf.master.clk_speed = cfg.freq_hz;
        
        esp_err_t err = i2c_param_config(port, &i2c_conf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2C %s param config failed: %s", cfg.id.c_str(), esp_err_to_name(err));
            return false;
        }
        
        err = i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2C %s driver install failed: %s", cfg.id.c_str(), esp_err_to_name(err));
            return false;
        }
        
        auto& res = i2c_buses_[i2c_bus_count_];
        res.id = cfg.id;
        res.port = port;
        res.initialized = true;
        i2c_bus_count_++;
        
        ESP_LOGI(TAG, "  I2C '%s' on SDA=%d SCL=%d @ %luHz",
                 cfg.id.c_str(), cfg.sda, cfg.scl, cfg.freq_hz);
    }
    return true;
}

bool HAL::init_i2c_expanders(const BoardConfig& config) {
    i2c_expander_count_ = 0;
    
    for (const auto& cfg : config.i2c_expanders) {
        if (i2c_expander_count_ >= MAX_I2C_EXPANDERS) break;
        
        // Знайти I2C bus
        i2c_port_t port = I2C_NUM_0;
        bool found_bus = false;
        for (size_t i = 0; i < i2c_bus_count_; i++) {
            if (i2c_buses_[i].id == cfg.bus_id) {
                port = i2c_buses_[i].port;
                found_bus = true;
                break;
            }
        }
        if (!found_bus) {
            ESP_LOGE(TAG, "I2C bus '%s' not found for expander '%s'",
                     cfg.bus_id.c_str(), cfg.id.c_str());
            return false;
        }
        
        auto& res = i2c_expanders_[i2c_expander_count_];
        res.id = cfg.id;
        res.i2c_port = port;
        res.address = cfg.address;
        res.pin_count = cfg.pin_count;
        res.output_state = 0x00;  // All OFF
        res.initialized = true;
        
        // Записати початковий стан (все OFF)
        res.write_state();
        
        i2c_expander_count_++;
        
        ESP_LOGI(TAG, "  I2C expander '%s' [%s] addr=0x%02X on bus '%s'",
                 cfg.id.c_str(), cfg.chip.c_str(), cfg.address, cfg.bus_id.c_str());
    }
    
    // Зберігаємо output/input configs
    expander_outputs_.clear();
    for (const auto& c : config.expander_outputs) {
        expander_outputs_.push_back(c);
    }
    expander_inputs_.clear();
    for (const auto& c : config.expander_inputs) {
        expander_inputs_.push_back(c);
    }
    
    return true;
}
```

### I2CExpanderResource::write_state() та read_state()

```cpp
bool I2CExpanderResource::write_state() {
    // PCF8574: один байт записує всі 8 виходів
    // ULN2003A інвертує: записуємо 1 для увімкнення реле
    uint8_t data = output_state;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return err == ESP_OK;
}

bool I2CExpanderResource::read_state(uint8_t& input_byte) {
    // PCF8574: один байт читає всі 8 входів
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &input_byte, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return err == ESP_OK;
}
```

### HAL::init() — додати виклики

```cpp
bool HAL::init(const BoardConfig& config) {
    // ...existing gpio_outputs and onewire...
    
    // I2C buses та expanders (тільки якщо є в config)
    if (!config.i2c_buses.empty()) {
        if (!init_i2c(config)) {
            ESP_LOGE(TAG, "I2C init failed");
            return false;
        }
        if (!init_i2c_expanders(config)) {
            ESP_LOGE(TAG, "I2C expander init failed");
            return false;
        }
    }
    
    ESP_LOGI(TAG, "HAL ready: %d gpio_out, %d ow, %d i2c_bus, %d i2c_exp",
             (int)gpio_output_count_, (int)onewire_count_,
             (int)i2c_bus_count_, (int)i2c_expander_count_);
    return true;
}
```

---

## Задача 3: PCF8574 Relay Driver

### Новий файл: drivers/pcf8574_relay/

Структура:
```
drivers/pcf8574_relay/
  ├── include/pcf8574_relay_driver.h
  ├── src/pcf8574_relay_driver.cpp
  ├── manifest.json
  └── CMakeLists.txt
```

### manifest.json

```json
{
  "manifest_version": 1,
  "driver": "pcf8574_relay",
  "description": "Реле через I2C expander PCF8574",
  "category": "actuator",
  "hardware_type": "i2c_expander_output",
  "requires_address": false,
  "multiple_per_bus": true,
  "provides": {"type": "bool"},
  "settings": []
}
```

### pcf8574_relay_driver.h

```cpp
#pragma once

#include "modesp/hal/driver_interfaces.h"
#include "modesp/hal/hal_types.h"
#include "etl/string.h"

class PCF8574RelayDriver : public modesp::IActuatorDriver {
public:
    PCF8574RelayDriver() = default;

    void configure(const char* role, modesp::I2CExpanderResource* expander,
                   uint8_t pin, bool active_high);

    bool init() override;
    void update(uint32_t dt_ms) override;
    bool set(bool state) override;
    bool get_state() const override { return relay_on_; }
    const char* role() const override { return role_.c_str(); }
    const char* type() const override { return "pcf8574_relay"; }
    bool is_healthy() const override { return initialized_ && expander_ != nullptr; }
    void emergency_stop() override;
    uint32_t switch_count() const override { return cycles_; }

private:
    etl::string<16> role_;
    modesp::I2CExpanderResource* expander_ = nullptr;
    uint8_t pin_          = 0;
    bool active_high_     = true;
    bool relay_on_        = false;
    bool initialized_     = false;
    bool configured_      = false;
    uint32_t cycles_      = 0;
};
```

### pcf8574_relay_driver.cpp

```cpp
#include "pcf8574_relay_driver.h"
#include "esp_log.h"

static const char* TAG = "PCF8574Relay";

void PCF8574RelayDriver::configure(const char* role,
                                     modesp::I2CExpanderResource* expander,
                                     uint8_t pin, bool active_high) {
    role_ = role;
    expander_ = expander;
    pin_ = pin;
    active_high_ = active_high;
    configured_ = true;
}

bool PCF8574RelayDriver::init() {
    if (!configured_ || !expander_) {
        ESP_LOGE(TAG, "Not configured");
        return false;
    }
    
    // Set initial state OFF
    if (active_high_) {
        expander_->output_state &= ~(1 << pin_);   // Clear bit = OFF
    } else {
        expander_->output_state |= (1 << pin_);     // Set bit = OFF
    }
    expander_->write_state();
    
    relay_on_ = false;
    initialized_ = true;
    
    ESP_LOGI(TAG, "[%s] Init on expander '%s' pin %d (active_%s)",
             role_.c_str(), expander_->id.c_str(), pin_,
             active_high_ ? "HIGH" : "LOW");
    return true;
}

void PCF8574RelayDriver::update(uint32_t dt_ms) {
    // PCF8574 relay не потребує periodic update
    // (на відміну від GPIO relay який відстежує min_switch_ms —
    //  тут це робить EquipmentModule на вищому рівні)
}

bool PCF8574RelayDriver::set(bool state) {
    if (state == relay_on_) return true;
    
    // Встановити/очистити біт у спільному output_state expander'а
    if (state) {
        if (active_high_) {
            expander_->output_state |= (1 << pin_);
        } else {
            expander_->output_state &= ~(1 << pin_);
        }
    } else {
        if (active_high_) {
            expander_->output_state &= ~(1 << pin_);
        } else {
            expander_->output_state |= (1 << pin_);
        }
    }
    
    // Записати в PCF8574 (один I2C write для всіх 8 каналів)
    if (!expander_->write_state()) {
        ESP_LOGE(TAG, "[%s] I2C write failed!", role_.c_str());
        return false;
    }
    
    relay_on_ = state;
    if (state) cycles_++;
    
    ESP_LOGI(TAG, "[%s] %s (cycle #%lu, byte=0x%02X)",
             role_.c_str(), state ? "ON" : "OFF",
             cycles_, expander_->output_state);
    return true;
}

void PCF8574RelayDriver::emergency_stop() {
    if (relay_on_) {
        ESP_LOGW(TAG, "[%s] EMERGENCY STOP", role_.c_str());
        set(false);
    }
}
```

---


## Задача 4: PCF8574 Digital Input Driver

### Новий файл: drivers/pcf8574_input/

```
drivers/pcf8574_input/
  ├── include/pcf8574_input_driver.h
  ├── src/pcf8574_input_driver.cpp
  ├── manifest.json
  └── CMakeLists.txt
```

### manifest.json

```json
{
  "manifest_version": 1,
  "driver": "pcf8574_input",
  "description": "Дискретний вхід через I2C expander PCF8574",
  "category": "sensor",
  "hardware_type": "i2c_expander_input",
  "requires_address": false,
  "multiple_per_bus": true,
  "provides": {"type": "bool"},
  "settings": [
    {"key": "invert", "type": "bool", "default": false, "description": "Інверсія логіки", "persist": true}
  ]
}
```

### pcf8574_input_driver.h

```cpp
#pragma once

#include "modesp/hal/driver_interfaces.h"
#include "modesp/hal/hal_types.h"
#include "etl/string.h"

class PCF8574InputDriver : public modesp::ISensorDriver {
public:
    PCF8574InputDriver() = default;

    void configure(const char* role, modesp::I2CExpanderResource* expander,
                   uint8_t pin, bool invert = false);

    bool init() override;
    void update(uint32_t dt_ms) override;
    bool read(float& value) override;   // 1.0f = active, 0.0f = inactive
    bool is_healthy() const override { return initialized_ && expander_ != nullptr; }
    const char* role() const override { return role_.c_str(); }
    const char* type() const override { return "pcf8574_input"; }

private:
    etl::string<16> role_;
    modesp::I2CExpanderResource* expander_ = nullptr;
    uint8_t pin_        = 0;
    bool invert_        = false;
    bool state_         = false;
    bool initialized_   = false;
    bool configured_    = false;
    uint32_t poll_ms_   = 0;
    static constexpr uint32_t POLL_INTERVAL_MS = 100;
};
```

### pcf8574_input_driver.cpp

```cpp
#include "pcf8574_input_driver.h"
#include "esp_log.h"

static const char* TAG = "PCF8574Input";

void PCF8574InputDriver::configure(const char* role,
                                     modesp::I2CExpanderResource* expander,
                                     uint8_t pin, bool invert) {
    role_ = role;
    expander_ = expander;
    pin_ = pin;
    invert_ = invert;
    configured_ = true;
}

bool PCF8574InputDriver::init() {
    if (!configured_ || !expander_) return false;
    
    initialized_ = true;
    ESP_LOGI(TAG, "[%s] Init on expander '%s' pin %d (invert=%s)",
             role_.c_str(), expander_->id.c_str(), pin_,
             invert_ ? "yes" : "no");
    return true;
}

void PCF8574InputDriver::update(uint32_t dt_ms) {
    poll_ms_ += dt_ms;
    if (poll_ms_ < POLL_INTERVAL_MS) return;
    poll_ms_ = 0;
    
    uint8_t input_byte = 0xFF;
    if (expander_->read_state(input_byte)) {
        // KC868-A6: оптоізольовані входи — LOW = активний (12V на вході)
        bool raw = (input_byte & (1 << pin_)) == 0;  // LOW = active
        state_ = invert_ ? !raw : raw;
    }
}

bool PCF8574InputDriver::read(float& value) {
    if (!initialized_) return false;
    value = state_ ? 1.0f : 0.0f;
    return true;
}
```

---

## Задача 5: DriverManager — створення I2C драйверів

### driver_manager.h — нові пули

```cpp
// Forward declarations
class PCF8574RelayDriver;
class PCF8574InputDriver;
```

### driver_manager.cpp — нові пули та create

Додати static пули:
```cpp
static PCF8574RelayDriver pcf_relay_pool[MAX_ACTUATORS];
static size_t pcf_relay_count = 0;

static PCF8574InputDriver pcf_input_pool[MAX_SENSORS];
static size_t pcf_input_count = 0;
```

В create_actuator():
```cpp
if (binding.driver_type == "pcf8574_relay") {
    if (pcf_relay_count >= MAX_ACTUATORS) {
        ESP_LOGE(TAG, "PCF8574 relay pool exhausted");
        return nullptr;
    }
    
    // Знайти expander output config
    auto* out_cfg = hal.find_expander_output(
        etl::string_view(binding.hardware_id.c_str(), binding.hardware_id.size()));
    if (!out_cfg) {
        ESP_LOGE(TAG, "Expander output '%s' not found", binding.hardware_id.c_str());
        return nullptr;
    }
    
    // Знайти expander resource
    auto* expander = hal.find_i2c_expander(
        etl::string_view(out_cfg->expander_id.c_str(), out_cfg->expander_id.size()));
    if (!expander) {
        ESP_LOGE(TAG, "Expander '%s' not found", out_cfg->expander_id.c_str());
        return nullptr;
    }
    
    auto& drv = pcf_relay_pool[pcf_relay_count++];
    drv.configure(binding.role.c_str(), expander, out_cfg->pin, out_cfg->active_high);
    return &drv;
}
```

Аналогічно в create_sensor() для "pcf8574_input".

---

## Задача 6: Config parser — читати нові секції board.json

### config_service.cpp — parse_board_json()

Додати парсинг секцій:
- `"i2c_buses"` → BoardConfig::i2c_buses
- `"i2c_expanders"` → BoardConfig::i2c_expanders  
- `"expander_outputs"` → BoardConfig::expander_outputs
- `"expander_inputs"` → BoardConfig::expander_inputs

Парсинг address як hex: "0x24" → 0x24 (strtol з base 16).

**ВАЖЛИВО:** Якщо ці секції відсутні в board.json — не помилка. Dev board без I2C працює як раніше.

---

## Задача 7: board.json та bindings.json для KC868-A6

### data/board_kc868a6.json

```json
{
  "manifest_version": 1,
  "board": "kincony_kc868_a6",
  "version": "1.0.0",
  "description": "KinCony KC868-A6 — 6 relay, 6 DI, I2C expander",
  "gpio_outputs": [],
  "i2c_buses": [
    {"id": "i2c_0", "sda": 4, "scl": 15, "freq_hz": 100000}
  ],
  "i2c_expanders": [
    {"id": "relay_exp", "bus": "i2c_0", "chip": "pcf8574", "address": "0x24", "pins": 8},
    {"id": "input_exp", "bus": "i2c_0", "chip": "pcf8574", "address": "0x22", "pins": 8}
  ],
  "expander_outputs": [
    {"id": "relay_1", "expander": "relay_exp", "pin": 0, "active_high": true},
    {"id": "relay_2", "expander": "relay_exp", "pin": 1, "active_high": true},
    {"id": "relay_3", "expander": "relay_exp", "pin": 2, "active_high": true},
    {"id": "relay_4", "expander": "relay_exp", "pin": 3, "active_high": true},
    {"id": "relay_5", "expander": "relay_exp", "pin": 4, "active_high": true},
    {"id": "relay_6", "expander": "relay_exp", "pin": 5, "active_high": true}
  ],
  "expander_inputs": [
    {"id": "din_1", "expander": "input_exp", "pin": 0},
    {"id": "din_2", "expander": "input_exp", "pin": 1},
    {"id": "din_3", "expander": "input_exp", "pin": 2},
    {"id": "din_4", "expander": "input_exp", "pin": 3},
    {"id": "din_5", "expander": "input_exp", "pin": 4},
    {"id": "din_6", "expander": "input_exp", "pin": 5}
  ],
  "onewire_buses": [
    {"id": "ow_1", "gpio": 32, "label": "OneWire шина 1"},
    {"id": "ow_2", "gpio": 33, "label": "OneWire шина 2"}
  ],
  "adc_channels": [
    {"id": "adc_1", "gpio": 36, "atten": 11, "label": "Analog 1 (0-5V)"},
    {"id": "adc_2", "gpio": 39, "atten": 11, "label": "Analog 2 (0-5V)"},
    {"id": "adc_3", "gpio": 34, "atten": 11, "label": "Analog 3 (0-5V)"},
    {"id": "adc_4", "gpio": 35, "atten": 11, "label": "Analog 4 (0-5V)"}
  ],
  "gpio_inputs": []
}
```

### data/bindings_kc868a6.json (приклад для холодильної камери)

```json
{
  "manifest_version": 1,
  "bindings": [
    {"hardware": "relay_1", "driver": "pcf8574_relay", "role": "compressor",   "module": "equipment"},
    {"hardware": "relay_2", "driver": "pcf8574_relay", "role": "evap_fan",     "module": "equipment"},
    {"hardware": "relay_3", "driver": "pcf8574_relay", "role": "cond_fan",     "module": "equipment"},
    {"hardware": "relay_4", "driver": "pcf8574_relay", "role": "defrost_heater","module": "equipment"},
    {"hardware": "relay_5", "driver": "pcf8574_relay", "role": "liquid_valve", "module": "equipment"},
    {"hardware": "relay_6", "driver": "pcf8574_relay", "role": "alarm_relay",  "module": "equipment"},
    {"hardware": "ow_1",    "driver": "ds18b20",       "role": "air_temp",     "module": "equipment"},
    {"hardware": "ow_1",    "driver": "ds18b20",       "role": "evap_temp",    "module": "equipment"},
    {"hardware": "din_1",   "driver": "pcf8574_input", "role": "door_switch",  "module": "equipment"},
    {"hardware": "din_2",   "driver": "pcf8574_input", "role": "hp_switch",    "module": "equipment"},
    {"hardware": "din_3",   "driver": "pcf8574_input", "role": "lp_switch",    "module": "equipment"},
    {"hardware": "din_4",   "driver": "pcf8574_input", "role": "night_input",  "module": "equipment"}
  ]
}
```

**Зверни увагу:** `relay_1..6` з `driver: "pcf8574_relay"` замість `driver: "relay"`.
Це єдина різниця від dev board — DriverManager створює інший клас драйвера.

---

## Задача 8: Тести

Додати тести в `tools/tests/`:

1. **test_board_config.py** — парсинг board_kc868a6.json:
   - i2c_buses парсяться
   - i2c_expanders парсяться з hex address
   - expander_outputs/inputs парсяться
   - gpio_outputs може бути порожнім

2. **test_bindings.py** — bindings_kc868a6.json:
   - pcf8574_relay driver type розпізнається
   - pcf8574_input driver type розпізнається
   - hardware_id матчиться з expander_outputs

3. **test_drivers.py** — маніфести нових драйверів:
   - pcf8574_relay manifest валідний
   - pcf8574_input manifest валідний
   - provides type correct

---

## Порядок реалізації

1. `hal_types.h` — нові struct (I2CBus, I2CExpander, configs)
2. `hal.h` / `hal.cpp` — init_i2c(), init_i2c_expanders(), find methods
3. `drivers/pcf8574_relay/` — новий драйвер (manifest, .h, .cpp, CMakeLists)
4. `drivers/pcf8574_input/` — новий драйвер (manifest, .h, .cpp, CMakeLists)
5. `driver_manager.cpp` — пули та create для нових driver types
6. `config_service.cpp` — парсинг нових секцій board.json
7. `data/board_kc868a6.json` — board config для KC868-A6
8. `data/bindings_kc868a6.json` — приклад bindings
9. Тести
10. CMakeLists.txt — підключити нові компоненти
11. `idf.py build`

## Що НЕ змінювати

- equipment_module.cpp — **БЕЗ ЗМІН**
- thermostat_module.cpp — БЕЗ ЗМІН
- defrost_module.cpp — БЕЗ ЗМІН
- protection_module.cpp — БЕЗ ЗМІН
- Існуючий relay_driver.cpp — БЕЗ ЗМІН
- Існуючий ds18b20_driver.cpp — БЕЗ ЗМІН
- WebUI — БЕЗ ЗМІН
- HTTP/WS/MQTT services — БЕЗ ЗМІН

## Перемикання між платами

Для переключення з dev board на KC868-A6 достатньо:
1. Скопіювати `board_kc868a6.json` → `board.json`
2. Скопіювати `bindings_kc868a6.json` → `bindings.json`
3. Перезавантажити контролер

**Одна прошивка — різні board.json.** Це головна перевага архітектури.

---

## Очікуваний результат

| Що | Деталі |
|----|--------|
| Нові файли | pcf8574_relay_driver.h/.cpp (~120 рядків), pcf8574_input_driver.h/.cpp (~80 рядків), 2 manifest.json, 2 CMakeLists |
| Змінені файли | hal_types.h (~40 рядків), hal.h (~20 рядків), hal.cpp (~80 рядків), driver_manager.cpp (~40 рядків), config_service.cpp (~60 рядків) |
| Board configs | board_kc868a6.json, bindings_kc868a6.json |
| Тести | ~15-20 нових |
| Backward compat | 100% — dev board працює без змін |

---

## Changelog
- 2026-02-21 — Створено. Phase 12a: KC868-A6 Board Support.
