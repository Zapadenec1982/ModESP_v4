# CLAUDE_TASK: Phase 12a — KC868-A6 Board Support (I2C Expander Drivers)

> Підтримка плати KinCony KC868-A6: реле та входи через I2C expander PCF8574.
> Прошивка залишається універсальною — board.json визначає яка плата.

---

## Статус виконання

| Задача | Опис | Статус |
|--------|------|--------|
| 1. HAL types | I2CBusConfig, I2CExpanderConfig, resources в hal_types.h | TODO |
| 2. HAL init | init_i2c(), init_i2c_expanders(), find methods в hal.h/.cpp | TODO |
| 3. PCF8574 Relay | Новий драйвер: IActuatorDriver через I2C expander | TODO |
| 4. PCF8574 Input | Новий драйвер: ISensorDriver через I2C expander | TODO |
| 5. DriverManager | Пули та create для pcf8574_relay/pcf8574_input | TODO |
| 6. config_service | Парсинг i2c_buses, i2c_expanders, expander_* з board.json | TODO |
| 7. board + bindings | board_kc868a6.json + bindings_kc868a6.json | TODO |
| 8. generate_ui.py | Нові hardware_types + BOARD_SECTION_TO_HW_TYPE | TODO |
| 9. CMakeLists | Підключити нові компоненти + esp_driver_i2c | TODO |
| 10. Тести | pytest для нових driver manifests + board config | TODO |

---

## Контекст

KC868-A6 — промислова ESP32 плата для автоматизації:
- 6 реле через **PCF8574 I2C expander** (адреса 0x24) + ULN2003A
- 6 оптоізольованих digital inputs через **PCF8574** (адреса 0x22)
- 2 OneWire шини: GPIO 32, GPIO 33 (pull-up 4.7K на платі)
- 4 ADC входи 0-5V: GPIO 36, 39, 34, 35
- I2C шина: SDA=4, SCL=15
- Живлення 12V DC

**Ключова різниця від dev board:** реле і входи НЕ на GPIO напряму, а через I2C expander.
Потрібні нові драйвери що реалізують ті самі інтерфейси `IActuatorDriver`/`ISensorDriver`.

**Бізнес-модулі (equipment, thermostat, defrost, protection) — БЕЗ ЗМІН.**
Вони працюють через `find_actuator("compressor")->set(true)` і їм байдуже
що всередині — GPIO чи I2C.

---

## Існуюча кодова база (reference)

### hal_types.h (рядки які зміняться)

```
Наявні config structs:  GpioOutputConfig, OneWireBusConfig, GpioInputConfig, AdcChannelConfig
Наявні resource structs: GpioOutputResource, OneWireBusResource, GpioInputResource, AdcChannelResource
BoardConfig: gpio_outputs, onewire_buses, gpio_inputs, adc_channels
Ліміти: MAX_RELAYS=8, MAX_ONEWIRE_BUSES=4, MAX_ADC_CHANNELS=8, MAX_BINDINGS=24
```

### hal.h / hal.cpp (патерн)

```
HAL::init(BoardConfig) -> init_gpio_outputs(), init_onewire(), init_gpio_inputs(), init_adc()
find_gpio_output(id), find_onewire_bus(id), find_gpio_input(id), find_adc_channel(id)
Масиви: etl::array<Resource, MAX> + size_t count_
```

### driver_manager.cpp (патерн)

```
Static pools: ds18b20_pool[MAX_SENSORS], relay_pool[MAX_ACTUATORS], di_pool, ntc_pool
init(): loop bindings -> if driver_type == "ds18b20" -> create_sensor()
                       -> if driver_type == "relay" -> create_actuator()
                       -> if driver_type == "digital_input" -> create_di_sensor()
                       -> if driver_type == "ntc" -> create_ntc_sensor()
create_*(): find resource in HAL -> pool[count++].configure(...) -> return &pool[count-1]
```

### IActuatorDriver (повний інтерфейс)

```cpp
init(), update(dt_ms), set(bool), get_state(), role(), type(),
is_healthy(), emergency_stop(), switch_count()
// + optional: set_value(float), get_value(), supports_analog()
```

### config_service.cpp — parse_board_json() (патерн)

```
jsmn parse -> loop tokens -> if jsoneq("gpio_outputs") -> inner loop:
  for each object: read "id", "gpio", "active_high" -> push_back(cfg)
Аналогічно для onewire_buses, gpio_inputs, adc_channels.
Невідомі секції ігноруються (skip_token).
```

### generate_ui.py (що потребує оновлення)

```python
VALID_HARDWARE_TYPES = {"gpio_output", "gpio_input", "onewire_bus", "adc_channel", "pwm_channel", "i2c_bus"}
BOARD_SECTION_TO_HW_TYPE = {
    "gpio_outputs": "gpio_output", "gpio_inputs": "gpio_input",
    "onewire_buses": "onewire_bus", "adc_channels": "adc_channel",
    "pwm_channels": "pwm_channel",
}
# Використовується для: hardware inventory, free hardware, bindings page roles
```

### CMakeLists.txt (modesp_hal)

```cmake
PRIV_REQUIRES ds18b20 relay digital_input ntc esp_adc
# Потрібно додати: pcf8574_relay pcf8574_input esp_driver_i2c
```

---

## Архітектура

```
board.json (KC868-A6)     board.json (dev board)
        |                         |
   I2C bus + expander config  GPIO output config
        |                         |
   HAL::init_i2c()            HAL::init_gpio_outputs()
   HAL::init_i2c_expanders()
        |                         |
   I2CExpanderResource        GpioOutputResource
        |                         |
   PCF8574RelayDriver         RelayDriver
        |                         |
   IActuatorDriver <------ Equipment Module ------> IActuatorDriver
   (та сама абстракція)                            (та сама абстракція)
```

**Ключовий момент:** PCF8574 — 8-bit I/O expander. Один I2C write записує ВСІ 8 пінів.
Тому `I2CExpanderResource` зберігає спільний `output_state` (бітова маска), а кожен
`PCF8574RelayDriver` змінює тільки свій біт і робить write всього байта.

---

## Задача 1: I2C типи в HAL

**Файл:** `components/modesp_hal/include/modesp/hal/hal_types.h`

### 1a. Нові ліміти

```cpp
static constexpr size_t MAX_I2C_BUSES      = 2;
static constexpr size_t MAX_I2C_EXPANDERS  = 4;
static constexpr size_t MAX_EXPANDER_IOS   = 16;  // Outputs + Inputs через expanders
```

### 1b. Board config structs

Додати ПІСЛЯ AdcChannelConfig:

```cpp
struct I2CBusConfig {
    HalId id;              // "i2c_0"
    gpio_num_t sda;
    gpio_num_t scl;
    uint32_t freq_hz;      // 100000 (100kHz standard)
};

struct I2CExpanderConfig {
    HalId id;              // "relay_exp" або "input_exp"
    HalId bus_id;          // "i2c_0"
    HalId chip;            // "pcf8574"
    uint8_t address;       // 0x24
    uint8_t pin_count;     // 8
};

struct I2CExpanderOutputConfig {
    HalId id;              // "relay_1" (hardware_id для bindings)
    HalId expander_id;     // "relay_exp"
    uint8_t pin;           // 0-7
    bool active_high;      // true для KC868-A6 + ULN2003A
};

struct I2CExpanderInputConfig {
    HalId id;              // "din_1"
    HalId expander_id;     // "input_exp"
    uint8_t pin;           // 0-7
    bool invert;           // false
};
```

### 1c. Додати в BoardConfig

```cpp
struct BoardConfig {
    // ...existing fields...
    etl::vector<I2CBusConfig, MAX_I2C_BUSES>                i2c_buses;
    etl::vector<I2CExpanderConfig, MAX_I2C_EXPANDERS>       i2c_expanders;
    etl::vector<I2CExpanderOutputConfig, MAX_EXPANDER_IOS>  expander_outputs;
    etl::vector<I2CExpanderInputConfig, MAX_EXPANDER_IOS>   expander_inputs;
};
```

### 1d. Resource structs

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

    /// Записати output_state в PCF8574 (один I2C byte write)
    bool write_state();
    /// Прочитати всі 8 входів з PCF8574 (один I2C byte read)
    bool read_state(uint8_t& input_byte);
};
```

**Потрібен include:** `#include "driver/i2c.h"` для `i2c_port_t`.
Щоб не тягнути цей header у всі файли що include hal_types.h — можна використати
`uint8_t port;` замість `i2c_port_t` і кастити при виклику.

---

## Задача 2: HAL — I2C ініціалізація

**Файли:** `components/modesp_hal/include/modesp/hal/hal.h`, `components/modesp_hal/src/hal.cpp`

### 2a. hal.h — нові public методи

```cpp
    I2CExpanderResource*      find_i2c_expander(etl::string_view id);
    I2CExpanderOutputConfig*  find_expander_output(etl::string_view id);
    I2CExpanderInputConfig*   find_expander_input(etl::string_view id);
    size_t i2c_expander_count() const { return i2c_expander_count_; }
```

### 2b. hal.h — нові private members

```cpp
    etl::array<I2CBusResource, MAX_I2C_BUSES>          i2c_buses_;
    etl::array<I2CExpanderResource, MAX_I2C_EXPANDERS> i2c_expanders_;
    // Configs зберігаються для lookup по hardware_id в DriverManager
    etl::vector<I2CExpanderOutputConfig, MAX_EXPANDER_IOS> expander_outputs_;
    etl::vector<I2CExpanderInputConfig, MAX_EXPANDER_IOS>  expander_inputs_;
    size_t i2c_bus_count_      = 0;
    size_t i2c_expander_count_ = 0;

    bool init_i2c(const BoardConfig& config);
    bool init_i2c_expanders(const BoardConfig& config);
```

### 2c. hal.cpp — init_i2c()

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

        ESP_RETURN_ON_ERROR(i2c_param_config(port, &i2c_conf), TAG,
                            "I2C %s param config failed", cfg.id.c_str());
        ESP_RETURN_ON_ERROR(i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0), TAG,
                            "I2C %s driver install failed", cfg.id.c_str());

        auto& res = i2c_buses_[i2c_bus_count_++];
        res.id = cfg.id;
        res.port = port;
        res.initialized = true;
        ESP_LOGI(TAG, "  I2C '%s' SDA=%d SCL=%d @ %luHz", cfg.id.c_str(), cfg.sda, cfg.scl, cfg.freq_hz);
    }
    return true;
}
```

### 2d. hal.cpp — init_i2c_expanders()

```cpp
bool HAL::init_i2c_expanders(const BoardConfig& config) {
    i2c_expander_count_ = 0;
    for (const auto& cfg : config.i2c_expanders) {
        if (i2c_expander_count_ >= MAX_I2C_EXPANDERS) break;
        // Знайти I2C bus по bus_id
        i2c_port_t port = I2C_NUM_0;
        bool found = false;
        for (size_t i = 0; i < i2c_bus_count_; i++) {
            if (i2c_buses_[i].id == cfg.bus_id) { port = i2c_buses_[i].port; found = true; break; }
        }
        if (!found) {
            ESP_LOGE(TAG, "I2C bus '%s' not found for expander '%s'", cfg.bus_id.c_str(), cfg.id.c_str());
            return false;
        }
        auto& res = i2c_expanders_[i2c_expander_count_++];
        res.id = cfg.id;
        res.i2c_port = port;
        res.address = cfg.address;
        res.pin_count = cfg.pin_count;
        res.output_state = 0x00;  // Все OFF
        res.initialized = true;
        // Записати початковий стан (все OFF)
        if (!res.write_state()) {
            ESP_LOGE(TAG, "Expander '%s' I2C write failed at 0x%02X — device not responding?",
                     cfg.id.c_str(), cfg.address);
            return false;
        }
        ESP_LOGI(TAG, "  I2C expander '%s' [%s] addr=0x%02X on '%s'",
                 cfg.id.c_str(), cfg.chip.c_str(), cfg.address, cfg.bus_id.c_str());
    }
    // Зберігаємо output/input configs для lookup
    expander_outputs_.clear();
    for (const auto& c : config.expander_outputs) expander_outputs_.push_back(c);
    expander_inputs_.clear();
    for (const auto& c : config.expander_inputs) expander_inputs_.push_back(c);
    return true;
}
```

### 2e. I2CExpanderResource::write_state() / read_state()

```cpp
bool I2CExpanderResource::write_state() {
    // PCF8574: один байт записує всі 8 виходів
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, output_state, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return err == ESP_OK;
}

bool I2CExpanderResource::read_state(uint8_t& input_byte) {
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

**Примітка щодо ESP-IDF I2C API:** ESP-IDF v5.x має новий I2C master API (`i2c_master_bus_*`).
Старий API (`i2c_cmd_link_create` / `i2c_master_cmd_begin`) позначений як legacy, але все ще працює
і є простішим. Для v5.5 legacy API — ОК, міграція на новий — можлива в майбутньому.

### 2f. HAL::init() — додати виклики

В `HAL::init()` після існуючих `init_adc()`:

```cpp
    // I2C buses та expanders (тільки якщо є в config)
    if (!config.i2c_buses.empty()) {
        if (!init_i2c(config)) return false;
        if (!init_i2c_expanders(config)) return false;
    }
    ESP_LOGI(TAG, "HAL ready: %d gpio_out, %d ow, %d gpio_in, %d adc, %d i2c_exp",
             (int)gpio_output_count_, (int)onewire_count_,
             (int)gpio_input_count_, (int)adc_count_, (int)i2c_expander_count_);
```

### 2g. find_*() методи

Такий самий лінійний пошук як існуючі find_gpio_output, find_onewire_bus:

```cpp
I2CExpanderResource* HAL::find_i2c_expander(etl::string_view id) {
    for (size_t i = 0; i < i2c_expander_count_; i++) {
        if (i2c_expanders_[i].id.size() == id.size() &&
            etl::string_view(i2c_expanders_[i].id.c_str(), i2c_expanders_[i].id.size()) == id)
            return &i2c_expanders_[i];
    }
    return nullptr;
}
// Аналогічно find_expander_output() і find_expander_input() по expander_outputs_/inputs_
```

---

## Задача 3: PCF8574 Relay Driver

**Директорія:** `drivers/pcf8574_relay/`

### 3a. manifest.json

```json
{
  "manifest_version": 1,
  "driver": "pcf8574_relay",
  "description": "Relay via I2C expander PCF8574",
  "category": "actuator",
  "hardware_type": "i2c_expander_output",
  "requires_address": false,
  "multiple_per_bus": true,
  "provides": {"type": "bool"},
  "settings": []
}
```

### 3b. pcf8574_relay_driver.h

```cpp
#pragma once
#include "modesp/hal/driver_interfaces.h"
#include "modesp/hal/hal_types.h"
#include "etl/string.h"

class PCF8574RelayDriver : public modesp::IActuatorDriver {
public:
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
    uint8_t pin_       = 0;
    bool active_high_  = true;
    bool relay_on_     = false;
    bool initialized_  = false;
    bool configured_   = false;
    uint32_t cycles_   = 0;
};
```

### 3c. pcf8574_relay_driver.cpp

```cpp
#include "pcf8574_relay_driver.h"
#include "esp_log.h"
static const char* TAG = "PCF8574Relay";

void PCF8574RelayDriver::configure(const char* role,
                                    modesp::I2CExpanderResource* expander,
                                    uint8_t pin, bool active_high) {
    role_ = role; expander_ = expander; pin_ = pin;
    active_high_ = active_high; configured_ = true;
}

bool PCF8574RelayDriver::init() {
    if (!configured_ || !expander_) return false;
    // Початковий стан OFF
    if (active_high_) expander_->output_state &= ~(1 << pin_);
    else              expander_->output_state |= (1 << pin_);
    expander_->write_state();
    relay_on_ = false;
    initialized_ = true;
    ESP_LOGI(TAG, "[%s] pin %d on '%s' (active_%s)",
             role_.c_str(), pin_, expander_->id.c_str(), active_high_ ? "HIGH" : "LOW");
    return true;
}

void PCF8574RelayDriver::update(uint32_t) {
    // PCF8574 relay не потребує periodic update.
    // Захист компресора від коротких циклів — на рівні EquipmentModule.
}

bool PCF8574RelayDriver::set(bool state) {
    if (state == relay_on_) return true;
    // Встановити/очистити біт у спільному output_state expander'а
    if (state == active_high_) expander_->output_state |= (1 << pin_);
    else                       expander_->output_state &= ~(1 << pin_);
    if (!expander_->write_state()) {
        ESP_LOGE(TAG, "[%s] I2C write failed!", role_.c_str());
        return false;
    }
    relay_on_ = state;
    if (state) cycles_++;
    ESP_LOGI(TAG, "[%s] %s (byte=0x%02X)", role_.c_str(), state ? "ON" : "OFF", expander_->output_state);
    return true;
}

void PCF8574RelayDriver::emergency_stop() {
    if (relay_on_) { ESP_LOGW(TAG, "[%s] EMERGENCY STOP", role_.c_str()); set(false); }
}
```

### 3d. CMakeLists.txt

```cmake
idf_component_register(
    SRCS "src/pcf8574_relay_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES modesp_hal
)
```

**Важливий нюанс set():** `active_high` спрощено до `if (state == active_high_)` —
коли state=ON і active_high=true -> set bit, коли state=ON і active_high=false -> clear bit.
Це правильно для KC868-A6 де ULN2003A інвертує вихід PCF8574.

---

## Задача 4: PCF8574 Input Driver

**Директорія:** `drivers/pcf8574_input/`

### 4a. manifest.json

```json
{
  "manifest_version": 1,
  "driver": "pcf8574_input",
  "description": "Digital input via I2C expander PCF8574",
  "category": "sensor",
  "hardware_type": "i2c_expander_input",
  "requires_address": false,
  "multiple_per_bus": true,
  "provides": {"type": "bool"},
  "settings": [
    {"key": "invert", "type": "bool", "default": false, "description": "Invert logic"}
  ]
}
```

### 4b. pcf8574_input_driver.h

```cpp
#pragma once
#include "modesp/hal/driver_interfaces.h"
#include "modesp/hal/hal_types.h"
#include "etl/string.h"

class PCF8574InputDriver : public modesp::ISensorDriver {
public:
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
    uint8_t pin_       = 0;
    bool invert_       = false;
    bool state_        = false;
    bool initialized_  = false;
    bool configured_   = false;
    uint32_t poll_ms_  = 0;
    static constexpr uint32_t POLL_INTERVAL_MS = 100;  // Кожні 100ms (I2C read)
};
```

### 4c. pcf8574_input_driver.cpp

```cpp
#include "pcf8574_input_driver.h"
#include "esp_log.h"
static const char* TAG = "PCF8574Input";

void PCF8574InputDriver::configure(const char* role,
                                    modesp::I2CExpanderResource* expander,
                                    uint8_t pin, bool invert) {
    role_ = role; expander_ = expander; pin_ = pin; invert_ = invert; configured_ = true;
}

bool PCF8574InputDriver::init() {
    if (!configured_ || !expander_) return false;
    initialized_ = true;
    ESP_LOGI(TAG, "[%s] pin %d on '%s' (invert=%s)",
             role_.c_str(), pin_, expander_->id.c_str(), invert_ ? "yes" : "no");
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

### 4d. CMakeLists.txt

```cmake
idf_component_register(
    SRCS "src/pcf8574_input_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES modesp_hal
)
```

**Оптимізація I2C reads:** Усі PCF8574InputDriver на одному expander читають той самий I2C байт.
Поточна реалізація — кожен робить окремий I2C read. Для 6 входів = 6 reads per 100ms.
I2C read при 100kHz ~ 0.3ms, тому 6 reads = 1.8ms — прийнятно.
Оптимізація (cache byte per expander) — можлива пізніше, не потрібна зараз.

---

## Задача 5: DriverManager — нові driver types

**Файли:** `components/modesp_hal/include/modesp/hal/driver_manager.h`, `components/modesp_hal/src/driver_manager.cpp`

### 5a. driver_manager.h — forward declarations

```cpp
class PCF8574RelayDriver;
class PCF8574InputDriver;
```

Додати private методи:

```cpp
    IActuatorDriver* create_pcf_actuator(const Binding& binding, HAL& hal);
    ISensorDriver*   create_pcf_sensor(const Binding& binding, HAL& hal);
```

### 5b. driver_manager.cpp — static pools

```cpp
#include "pcf8574_relay_driver.h"
#include "pcf8574_input_driver.h"

static PCF8574RelayDriver pcf_relay_pool[MAX_ACTUATORS];
static size_t pcf_relay_count = 0;

static PCF8574InputDriver pcf_input_pool[MAX_SENSORS];
static size_t pcf_input_count = 0;
```

### 5c. init() — нові гілки

В циклі bindings (після існуючих `else if`):

```cpp
    } else if (binding.driver_type == "pcf8574_relay") {
        IActuatorDriver* drv = create_pcf_actuator(binding, hal);
        if (drv) {
            // ... та ж логіка що для "relay" — push_back ActuatorEntry
        } else { return false; }
    } else if (binding.driver_type == "pcf8574_input") {
        if (!add_sensor(create_pcf_sensor(binding, hal), binding)) { return false; }
    }
```

### 5d. create_pcf_actuator()

```cpp
IActuatorDriver* DriverManager::create_pcf_actuator(const Binding& binding, HAL& hal) {
    if (pcf_relay_count >= MAX_ACTUATORS) { ESP_LOGE(TAG, "PCF relay pool exhausted"); return nullptr; }
    // Знайти expander output config по hardware_id ("relay_1")
    auto* out_cfg = hal.find_expander_output(
        etl::string_view(binding.hardware_id.c_str(), binding.hardware_id.size()));
    if (!out_cfg) { ESP_LOGE(TAG, "Expander output '%s' not found", binding.hardware_id.c_str()); return nullptr; }
    // Знайти expander resource по expander_id ("relay_exp")
    auto* expander = hal.find_i2c_expander(
        etl::string_view(out_cfg->expander_id.c_str(), out_cfg->expander_id.size()));
    if (!expander) { ESP_LOGE(TAG, "Expander '%s' not found", out_cfg->expander_id.c_str()); return nullptr; }

    auto& drv = pcf_relay_pool[pcf_relay_count++];
    drv.configure(binding.role.c_str(), expander, out_cfg->pin, out_cfg->active_high);
    return &drv;
}
```

### 5e. create_pcf_sensor()

Аналогічно: find_expander_input() -> find_i2c_expander() -> configure().

### 5f. Reset в init()

```cpp
pcf_relay_count = 0;
pcf_input_count = 0;
```

---

## Задача 6: config_service — парсинг I2C секцій

**Файл:** `components/modesp_services/src/config_service.cpp`

Додати в `parse_board_json()` після існуючого парсингу `adc_channels`, за тим самим патерном.

### 6a. i2c_buses

```cpp
} else if (jsoneq(buf, &tokens[i], "i2c_buses")) {
    if (tokens[i + 1].type != JSMN_ARRAY) { ESP_LOGE(TAG, "'i2c_buses' not array"); return false; }
    int arr_size = tokens[i + 1].size;
    int j = i + 2;
    for (int a = 0; a < arr_size; a++) {
        I2CBusConfig cfg = {};
        int obj_size = tokens[j].size;
        j++;
        for (int k = 0; k < obj_size; k++) {
            if (jsoneq(buf, &tokens[j], "id"))       { tok_to_str(buf, &tokens[j+1], tmp, sizeof(tmp)); cfg.id = tmp; }
            else if (jsoneq(buf, &tokens[j], "sda"))  { cfg.sda = (gpio_num_t)tok_to_int(buf, &tokens[j+1]); }
            else if (jsoneq(buf, &tokens[j], "scl"))  { cfg.scl = (gpio_num_t)tok_to_int(buf, &tokens[j+1]); }
            else if (jsoneq(buf, &tokens[j], "freq_hz")) { cfg.freq_hz = (uint32_t)tok_to_int(buf, &tokens[j+1]); }
            j += 2;
        }
        if (!board_config_.i2c_buses.full()) board_config_.i2c_buses.push_back(cfg);
    }
    i = j - 1;
```

### 6b. i2c_expanders

Поле `address` як hex string: `"0x24"` -> `strtol(tmp, nullptr, 16)`.

```cpp
else if (jsoneq(buf, &tokens[j], "address")) {
    tok_to_str(buf, &tokens[j+1], tmp, sizeof(tmp));
    cfg.address = (uint8_t)strtol(tmp, nullptr, 16);
}
else if (jsoneq(buf, &tokens[j], "pins")) { cfg.pin_count = (uint8_t)tok_to_int(buf, &tokens[j+1]); }
```

### 6c. expander_outputs

```cpp
// Поля: "id", "expander", "pin", "active_high"
```

### 6d. expander_inputs

```cpp
// Поля: "id", "expander", "pin", "invert" (optional, default false)
```

### 6e. Оновити boot log

```cpp
ESP_LOGI(TAG, "Board: %s v%s (%d gpio_out, %d ow, %d gpio_in, %d adc, %d i2c_bus, %d i2c_exp)",
         ..., (int)board_config_.i2c_buses.size(), (int)board_config_.i2c_expanders.size());
```

**Backward compatible:** Якщо `i2c_buses` відсутній в board.json — просто 0 елементів.
Dev board працює без змін.

---

## Задача 7: board.json та bindings.json для KC868-A6

### 7a. data/board_kc868a6.json

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
    {"id": "ow_1", "gpio": 32, "label": "OneWire 1"},
    {"id": "ow_2", "gpio": 33, "label": "OneWire 2"}
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

### 7b. data/bindings_kc868a6.json (приклад для холодильної камери)

```json
{
  "manifest_version": 1,
  "bindings": [
    {"hardware": "relay_1", "driver": "pcf8574_relay", "role": "compressor",  "module": "equipment"},
    {"hardware": "relay_2", "driver": "pcf8574_relay", "role": "evap_fan",    "module": "equipment"},
    {"hardware": "relay_3", "driver": "pcf8574_relay", "role": "cond_fan",    "module": "equipment"},
    {"hardware": "relay_4", "driver": "pcf8574_relay", "role": "heater",      "module": "equipment"},
    {"hardware": "relay_5", "driver": "pcf8574_relay", "role": "hg_valve",    "module": "equipment"},
    {"hardware": "ow_1",    "driver": "ds18b20",       "role": "air_temp",    "module": "equipment", "address": "28:FF:AA:BB:CC:DD:EE:01"},
    {"hardware": "ow_1",    "driver": "ds18b20",       "role": "evap_temp",   "module": "equipment", "address": "28:FF:BB:CC:DD:EE:FF:02"},
    {"hardware": "din_1",   "driver": "pcf8574_input", "role": "door_contact","module": "equipment"},
    {"hardware": "din_2",   "driver": "pcf8574_input", "role": "night_input", "module": "equipment"}
  ]
}
```

**Різниця від dev board:** `driver: "pcf8574_relay"` замість `driver: "relay"`.
DriverManager створює PCF8574RelayDriver замість RelayDriver — бізнес-модулі не знають різниці.

---

## Задача 8: generate_ui.py

**Файл:** `tools/generate_ui.py`

### 8a. Нові hardware_types

```python
VALID_HARDWARE_TYPES = {
    "gpio_output", "gpio_input", "onewire_bus",
    "adc_channel", "pwm_channel", "i2c_bus",
    "i2c_expander_output", "i2c_expander_input",  # NEW
}
```

### 8b. Нові секції в BOARD_SECTION_TO_HW_TYPE

```python
BOARD_SECTION_TO_HW_TYPE = {
    "gpio_outputs": "gpio_output",
    "gpio_inputs": "gpio_input",
    "onewire_buses": "onewire_bus",
    "adc_channels": "adc_channel",
    "pwm_channels": "pwm_channel",
    "expander_outputs": "i2c_expander_output",   # NEW
    "expander_inputs": "i2c_expander_input",     # NEW
}
```

Це автоматично забезпечує:
- Hardware inventory для BindingsEditor (roles -> compatible hardware)
- Free hardware виявлення
- Bindings page формування

### 8c. Cross-validation

В `CrossValidator` додати перевірку: якщо binding.driver = "pcf8574_relay" ->
hardware_id повинен бути в `expander_outputs` (а не `gpio_outputs`).

---

## Задача 9: CMakeLists.txt

### 9a. Нові driver компоненти

Створити `drivers/pcf8574_relay/CMakeLists.txt` і `drivers/pcf8574_input/CMakeLists.txt` (вже описані в Задачах 3d, 4d).

### 9b. modesp_hal CMakeLists

```cmake
idf_component_register(
    SRCS "src/hal.cpp" "src/driver_manager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES modesp_core driver
    PRIV_REQUIRES ds18b20 relay digital_input ntc pcf8574_relay pcf8574_input esp_adc
)
```

**Питання: чи потрібен esp_driver_i2c?**
I2C API включається через `#include "driver/i2c.h"` який є частиною ESP-IDF `driver` component.
`REQUIRES driver` вже є. Але в ESP-IDF v5.x I2C перенесено в окремий `esp_driver_i2c`.
Перевірити при build — якщо `i2c_param_config` не знайдений, додати `esp_driver_i2c`.

---

## Задача 10: Тести

**Файл:** `tools/tests/test_features.py` або новий `test_kc868.py`

### 10a. Driver manifests

```python
def test_pcf8574_relay_manifest():
    m = load_json("drivers/pcf8574_relay/manifest.json")
    assert m["driver"] == "pcf8574_relay"
    assert m["category"] == "actuator"
    assert m["hardware_type"] == "i2c_expander_output"

def test_pcf8574_input_manifest():
    m = load_json("drivers/pcf8574_input/manifest.json")
    assert m["driver"] == "pcf8574_input"
    assert m["category"] == "sensor"
    assert m["hardware_type"] == "i2c_expander_input"
```

### 10b. Board config parsing

```python
def test_kc868a6_board():
    board = load_json("data/board_kc868a6.json")
    assert len(board["i2c_buses"]) == 1
    assert board["i2c_buses"][0]["sda"] == 4
    assert len(board["i2c_expanders"]) == 2
    assert len(board["expander_outputs"]) == 6
    assert len(board["expander_inputs"]) == 6
    assert board["gpio_outputs"] == []  # Немає GPIO outputs

def test_kc868a6_bindings():
    board = load_json("data/board_kc868a6.json")
    bindings = load_json("data/bindings_kc868a6.json")
    # pcf8574_relay hardware має бути в expander_outputs
    exp_out_ids = {o["id"] for o in board["expander_outputs"]}
    for b in bindings["bindings"]:
        if b["driver"] == "pcf8574_relay":
            assert b["hardware"] in exp_out_ids
```

### 10c. generate_ui.py з KC868-A6 board

```python
def test_generate_with_kc868():
    """Run generator with KC868-A6 board+bindings - no errors."""
    # Підставити board_kc868a6.json + bindings_kc868a6.json
    # Перевірити що генерація проходить без помилок
```

---

## Порядок реалізації

1. **hal_types.h** — нові struct (I2CBus, I2CExpander, configs, resources) — ~50 рядків
2. **hal.h / hal.cpp** — init_i2c(), find methods — ~100 рядків
3. **drivers/pcf8574_relay/** — manifest, .h, .cpp, CMakeLists — ~80 рядків
4. **drivers/pcf8574_input/** — manifest, .h, .cpp, CMakeLists — ~60 рядків
5. **driver_manager.h/.cpp** — pools, create methods, init гілки — ~50 рядків
6. **config_service.cpp** — парсинг i2c_buses/expanders/outputs/inputs — ~80 рядків
7. **generate_ui.py** — VALID_HARDWARE_TYPES + BOARD_SECTION_TO_HW_TYPE — ~10 рядків
8. **data/board_kc868a6.json** — board config
9. **data/bindings_kc868a6.json** — bindings example
10. **CMakeLists.txt** — підключити нові компоненти
11. **Тести** — ~15-20 нових
12. **Build + verify:** `python tools/generate_ui.py && python -m pytest tools/tests/ -v && idf.py build`

---

## Що НЕ змінювати

- equipment_module.cpp — **БЕЗ ЗМІН** (IActuatorDriver абстракція)
- thermostat_module.cpp — БЕЗ ЗМІН
- defrost_module.cpp — БЕЗ ЗМІН
- protection_module.cpp — БЕЗ ЗМІН
- Існуючий relay_driver — БЕЗ ЗМІН
- ds18b20_driver — БЕЗ ЗМІН
- WebUI — БЕЗ ЗМІН (BindingsEditor вже підтримує будь-які hardware types)
- HTTP/WS/MQTT services — БЕЗ ЗМІН

---

## Перемикання між платами

```bash
# Для KC868-A6:
cp data/board_kc868a6.json data/board.json
cp data/bindings_kc868a6.json data/bindings.json
# Reboot

# Для dev board — повернути оригінальні файли
```

**Одна прошивка — різні board.json.** Це головна перевага архітектури.

---

## Очікуваний обсяг

| Компонент | Файли | Рядки (~) |
|-----------|-------|-----------|
| hal_types.h | MODIFY | +50 |
| hal.h + hal.cpp | MODIFY | +100 |
| pcf8574_relay driver | CREATE (4 files) | ~80 |
| pcf8574_input driver | CREATE (4 files) | ~60 |
| driver_manager.h/.cpp | MODIFY | +50 |
| config_service.cpp | MODIFY | +80 |
| generate_ui.py | MODIFY | +10 |
| Board/bindings JSON | CREATE (2 files) | ~60 |
| Тести | CREATE/MODIFY | ~50 |
| CMakeLists | MODIFY | +5 |
| **Разом** | ~14 файлів | **~545 рядків** |

---

## Ризики і нюанси

1. **ESP-IDF I2C API version:** v5.x має legacy і new API. Legacy (`i2c_cmd_link_create`) простіший
   і достатній. New API (`i2c_master_bus_*`) — міграція пізніше.

2. **PCF8574 не відповідає:** init_i2c_expanders() робить write_state() — якщо I2C device
   не на шині, init провалиться з логом. Це правильна поведінка — краще впасти при boot
   ніж мовчки не працювати.

3. **Shared output_state:** Кілька PCF8574RelayDriver на одному expander змінюють спільний
   output_state. Це безпечно бо все працює з одного main loop task (single-threaded update).
   HTTPD task ніколи не пише relay напряму.

4. **GPIO 15 конфлікт:** KC868-A6 використовує GPIO 15 для I2C SCL. Dev board використовує
   GPIO 15 для OneWire. board.json визначає використання — конфлікту немає якщо правильний board.json.

5. **ADC 0-5V:** KC868-A6 має внутрішній дільник для ADC (5V -> 3.3V range). atten=11 (0-3.3V)
   потрібно масштабувати для 0-5V. Це питання NTC/ADC driver calibration, не Phase 12a.

---

## Changelog
- 2026-02-23 — Оновлено: додано таблицю статусу, reference до існуючого коду (hal_types.h structs,
  hal.h/hal.cpp patterns, driver_manager.cpp create pattern, config_service.cpp jsmn parse pattern,
  generate_ui.py VALID_HARDWARE_TYPES/BOARD_SECTION_TO_HW_TYPE, CMakeLists.txt PRIV_REQUIRES,
  IActuatorDriver interface, relay_driver.h pattern). Додано Задачу 8 (generate_ui.py),
  Задачу 9 (CMakeLists). Деталізовано ризики/нюанси (I2C API, shared output_state, GPIO 15).
  Спрощено set() логіку PCF8574RelayDriver (state == active_high_).
- 2026-02-21 — Створено. Phase 12a: KC868-A6 Board Support.
