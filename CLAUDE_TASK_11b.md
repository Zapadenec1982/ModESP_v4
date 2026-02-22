# CLAUDE_TASK: Phase 11b — OneWire Multi-Sensor (Auto-learn)

> Підтримка кількох DS18B20 на одній OneWire шині з послідовним додаванням через WebUI.
> Scan → показати нові датчики з температурою → призначити роль → зберегти в bindings.json.

---

## Контекст

Зараз DS18B20 драйвер використовує `SKIP_ROM` (0xCC) — адресує ВСІ пристрої на шині.
Це працює тільки якщо на шині один датчик. Потрібно:

1. SEARCH_ROM для виявлення всіх 64-bit адрес на шині
2. MATCH_ROM (0x55) + адреса для читання конкретного датчика
3. WebUI для послідовного додавання (auto-learn)
4. Збереження адрес в bindings.json

**Backward compatible:** якщо address відсутній в binding → SKIP_ROM як раніше (один датчик).

---

## Огляд існуючої архітектури

### Файли що змінюються

| Файл | Що змінюється |
|------|--------------|
| `components/modesp_hal/include/modesp/hal/hal_types.h` | Binding struct: додати поле address |
| `components/modesp_services/src/config_service.cpp` | Парсер: читати "address" з bindings.json |
| `drivers/ds18b20/include/ds18b20_driver.h` | Додати rom_address_, SEARCH_ROM, MATCH_ROM |
| `drivers/ds18b20/src/ds18b20_driver.cpp` | MATCH_ROM в start_conversion/read_scratchpad, scan API |
| `components/modesp_hal/src/driver_manager.cpp` | Передавати address в ds18b20 configure() |
| `components/modesp_services/src/http_handlers.cpp` | Новий endpoint GET/POST /api/onewire/* |
| `webui/src/pages/Bindings.svelte` (або новий) | UI для scan + assign |
| `data/bindings.json` | Формат: додається поле "address" |

### Файли що НЕ змінюються

- equipment_module.cpp — він вже робить find_sensor("air_temp"), йому байдуже на адреси
- thermostat/defrost/protection modules — нічого не знають про датчики
- board.json — OneWire шини вже описані (gpio, id)

---

## Задача 1: Binding struct — додати поле address

### hal_types.h

```cpp
struct Binding {
    HalId      hardware_id;   // "ow_1" — шина
    Role       role;           // "air_temp"
    DriverType driver_type;    // "ds18b20"
    ModuleName module_name;    // "equipment"
    etl::string<24> address;   // "28FFA1B3CCDDEE01" (hex, 16 символів для 8 байт)
                                // Порожній → SKIP_ROM (backward compatible)
};
```

Адреса зберігається як hex string без роздільників (16 hex chars = 8 bytes).
Це простіше парсити ніж "28:FF:A1:B3:..." і не потребує окремого формату.

---

## Задача 2: Парсер bindings.json — читати address

### config_service.cpp

В циклі парсингу binding object, додати ще один `else if`:

```cpp
} else if (jsoneq(buf, &tokens[j], "address")) {
    tok_to_str(buf, &tokens[j + 1], tmp, sizeof(tmp));
    binding.address = tmp;
    j += 2;
}
```

bindings.json тепер:
```json
{"hardware": "ow_1", "driver": "ds18b20", "role": "air_temp",  "module": "equipment", "address": "28FFA1B3CCDDEE01"},
{"hardware": "ow_1", "driver": "ds18b20", "role": "evap_temp", "module": "equipment", "address": "28FFB2C4DDEEAA02"}
```

Обидва датчики на ОДНІЙ шині "ow_1" (GPIO 15) але з різними адресами.

Backward compatible: якщо "address" відсутній → `binding.address` залишається порожнім → SKIP_ROM.

---

## Задача 3: DS18B20 Driver — MATCH_ROM + SEARCH_ROM

### ds18b20_driver.h — нові поля і методи

```cpp
class DS18B20Driver : public modesp::ISensorDriver {
public:
    // Оновлений configure — приймає опціональну адресу
    void configure(const char* role, gpio_num_t gpio, 
                   uint32_t read_interval_ms = 1000,
                   const char* rom_address_hex = nullptr);

    // ── Scan API (статичний, працює з будь-якою шиною) ──
    struct RomAddress {
        uint8_t bytes[8];
    };
    static constexpr size_t MAX_DEVICES_PER_BUS = 8;

    /// Сканує OneWire шину, повертає кількість знайдених пристроїв
    static size_t scan_bus(gpio_num_t gpio, RomAddress* results, size_t max_results);

    /// Читає температуру з конкретного датчика (для preview при скануванні)
    static bool read_temp_by_address(gpio_num_t gpio, const RomAddress& addr, float& temp_out);

private:
    // ── ROM address ──
    uint8_t rom_address_[8] = {};
    bool    has_address_ = false;   // true → MATCH_ROM, false → SKIP_ROM

    // ── OneWire SEARCH ROM ──
    static bool search_rom(gpio_num_t gpio, RomAddress* results, size_t max_results, size_t& found);

    // ── Address helpers ──
    static bool parse_hex_address(const char* hex, uint8_t* out);     // "28FF..." → bytes
    static void format_hex_address(const uint8_t* addr, char* out);   // bytes → "28FF..."
    void write_address();  // MATCH_ROM + 8 bytes або SKIP_ROM

    // ...existing members...
};
```

### ds18b20_driver.cpp — зміни

**configure()** — парсить hex адресу:
```cpp
void DS18B20Driver::configure(const char* role, gpio_num_t gpio,
                               uint32_t read_interval_ms,
                               const char* rom_address_hex) {
    role_ = role;
    gpio_ = gpio;
    read_interval_ms_ = read_interval_ms;
    configured_ = true;

    if (rom_address_hex && strlen(rom_address_hex) == 16) {
        has_address_ = parse_hex_address(rom_address_hex, rom_address_);
        if (has_address_) {
            ESP_LOGI(TAG, "[%s] ROM address: %s", role, rom_address_hex);
        }
    } else {
        has_address_ = false;
        ESP_LOGI(TAG, "[%s] No address → SKIP_ROM (single sensor mode)", role);
    }
}
```

**write_address()** — обирає MATCH_ROM або SKIP_ROM:
```cpp
void DS18B20Driver::write_address() {
    if (has_address_) {
        onewire_write_byte(0x55);  // MATCH_ROM
        for (int i = 0; i < 8; i++) {
            onewire_write_byte(rom_address_[i]);
        }
    } else {
        onewire_write_byte(0xCC);  // SKIP_ROM
    }
}
```

**start_conversion()** — замінити SKIP_ROM на write_address():
```cpp
bool DS18B20Driver::start_conversion() {
    if (!onewire_reset()) return false;
    write_address();
    onewire_write_byte(CMD_CONVERT_T);
    return true;
}
```

**read_scratchpad()** — замінити SKIP_ROM на write_address():
```cpp
bool DS18B20Driver::read_scratchpad(uint8_t* buf, size_t len) {
    if (!onewire_reset()) return false;
    write_address();
    onewire_write_byte(CMD_READ_SCRATCH);
    for (size_t i = 0; i < len; i++) {
        buf[i] = onewire_read_byte();
    }
    return true;
}
```

**УВАГА при start_conversion() з SKIP_ROM:**
Коли кілька датчиків на шині БЕЗ адрес — SKIP_ROM надсилає CONVERT_T всім.
Це нормально: всі конвертують одночасно, потім кожен читається окремо по MATCH_ROM.
Але якщо хоча б один binding з адресою — всі мають мати адреси.

**Оптимізація conversion:** Якщо всі датчики на одній шині, можна зробити ОДИН
SKIP_ROM + CONVERT_T, зачекати 750ms, потім читати кожен по MATCH_ROM.
Це потребує координації в DriverManager.update_all() — НЕ робити зараз, залишити
як TODO для оптимізації. Поки кожен драйвер робить свій conversion окремо.

---

## Задача 4: SEARCH_ROM алгоритм (scan)

Dallas 1-Wire SEARCH ROM (0xF0) — стандартний алгоритм:

```cpp
size_t DS18B20Driver::scan_bus(gpio_num_t gpio, RomAddress* results, size_t max_results) {
    size_t found = 0;
    search_rom(gpio, results, max_results, found);
    return found;
}

bool DS18B20Driver::search_rom(gpio_num_t gpio, RomAddress* results, 
                                 size_t max_results, size_t& found) {
    // Standard 1-Wire Search ROM algorithm
    // References: Maxim AN187 "1-Wire Search Algorithm"
    //
    // Для кожного з 64 біт:
    //   1. Master читає біт (true value)
    //   2. Master читає complement
    //   3. Master пише direction (обираючи гілку)
    //
    // При конфлікті (bit=0, complement=0) → є пристрої з 0 і з 1
    // Запам'ятовуємо позицію конфлікту, йдемо спочатку по 0, потім повторюємо з 1

    uint8_t last_discrepancy = 0;
    bool last_device = false;
    found = 0;

    while (!last_device && found < max_results) {
        uint8_t rom[8] = {};
        uint8_t discrepancy_marker = 0;

        // GPIO config (тимчасово, якщо ще не налаштований)
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << gpio);
        io_conf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
        gpio_set_level(gpio, 1);

        // Reset
        gpio_set_level(gpio, 0);
        ets_delay_us(480);
        portDISABLE_INTERRUPTS();
        gpio_set_level(gpio, 1);
        ets_delay_us(70);
        int presence = gpio_get_level(gpio);
        portENABLE_INTERRUPTS();
        ets_delay_us(410);

        if (presence != 0) break;  // No devices

        // Send SEARCH ROM command
        // write_byte — потрібно inline бо static метод без instance
        // ... (використати inline OneWire helpers)

        // ... (64-bit search loop)
        
        // CRC8 check на знайдену адресу
        if (crc8(rom, 7) == rom[7]) {
            memcpy(results[found].bytes, rom, 8);
            found++;
        }

        if (last_discrepancy == 0) last_device = true;
    }

    return found > 0;
}
```

**ВАЖЛИВО:** SEARCH_ROM алгоритм складний (~80 рядків). Рекомендую взяти
реалізацію з Maxim AN187 або існуючих ESP32 OneWire бібліотек і адаптувати
під наш bit-bang з portDISABLE_INTERRUPTS. Ключові кроки:

1. Reset bus
2. Send 0xF0 (SEARCH ROM)  
3. Для кожного з 64 біт: read bit, read complement, write direction
4. При conflict → запам'ятати позицію для наступного проходу
5. CRC8 перевірка знайденої адреси
6. Повторити поки є конфлікти

Static методи scan_bus() і read_temp_by_address() використовуються
HTTP handler'ом для scan endpoint — їм не потрібен екземпляр драйвера.

---

## Задача 5: DriverManager — передати address

### driver_manager.cpp

```cpp
ISensorDriver* DriverManager::create_sensor(const Binding& binding, HAL& hal) {
    if (binding.driver_type == "ds18b20") {
        if (ds18b20_count >= MAX_SENSORS) {
            ESP_LOGE(TAG, "DS18B20 pool exhausted");
            return nullptr;
        }

        auto* ow_res = hal.find_onewire_bus(...);
        if (!ow_res) { ... }

        auto& drv = ds18b20_pool[ds18b20_count++];

        // Передаємо address якщо є
        const char* addr = binding.address.empty() ? nullptr : binding.address.c_str();
        drv.configure(binding.role.c_str(), ow_res->gpio, 1000, addr);
        return &drv;
    }
    return nullptr;
}
```

---

## Задача 6: HTTP endpoints для scan + assign

### Новий файл або додати в http_handlers.cpp


**GET /api/onewire/scan?bus=ow_1**

Сканує шину, повертає знайдені датчики з температурою і статусом:

```json
{
  "bus": "ow_1",
  "gpio": 15,
  "sensors": [
    {
      "address": "28FFA1B3CCDDEE01",
      "temperature": 4.5,
      "role": "air_temp",
      "status": "assigned"
    },
    {
      "address": "28FFB2C4DDEEAA02",
      "temperature": -18.3,
      "role": "evap_temp",
      "status": "assigned"
    },
    {
      "address": "28FFD3E5EEFF0003",
      "temperature": 28.7,
      "role": null,
      "status": "new"
    }
  ]
}
```

Алгоритм:
1. HAL → знайти gpio для bus_id
2. DS18B20Driver::scan_bus(gpio, ...) → список адрес
3. Для кожної адреси: DS18B20Driver::read_temp_by_address(gpio, addr, temp)
4. Порівняти з bindings → assigned/new
5. Повернути JSON

Температура допомагає ідентифікувати: датчик в руках ~28-30°C, на випарнику ~-20°C.

**POST /api/onewire/assign**

Призначає роль знайденому датчику:

```json
{
  "bus": "ow_1",
  "address": "28FFD3E5EEFF0003",
  "role": "condenser_temp",
  "module": "equipment"
}
```

Алгоритм:
1. Валідація: адреса 16 hex chars, роль не зайнята, модуль існує
2. Додати новий binding в bindings.json (LittleFS write)
3. Повернути 200 OK
4. Виставити прапорець "потрібен перезапуск" (bindings → DriverManager)

```json
{"ok": true, "message": "Sensor assigned. Restart to apply."}
```

**POST /api/onewire/unassign**

Зняти роль з датчика:

```json
{"address": "28FFD3E5EEFF0003"}
```

Видалити binding з bindings.json. Теж потребує restart.

**Перезапуск vs hot-reload:**
Найпростіше — вимагати restart після assign/unassign. Hot-reload DriverManager
складний (деструктори, перевстановлення GPIO). Для Phase 11b restart — ОК.
WebUI показує банер "Bindings змінено — перезавантажте контролер [Restart]".

---

## Задача 7: WebUI — сторінка Bindings з auto-learn

### Нова сторінка або секція на Settings page

**Структура:**

```
┌─────────────────────────────────────────────────────┐
│  🔌 Датчики OneWire                                 │
│                                                     │
│  Шина: ow_1 (GPIO 15)                              │
│                                                     │
│  ── Підключені ──────────────────────────────────  │
│                                                     │
│  28:FF:A1:B3:CC:DD:EE:01                           │
│  ✅ Датчик камери (air_temp)         T = 4.5°C     │
│                                   [🗑 Відв'язати]  │
│                                                     │
│  28:FF:B2:C4:DD:EE:AA:02                           │
│  ✅ Датчик випарника (evap_temp)     T = -18.3°C   │
│                                   [🗑 Відв'язати]  │
│                                                     │
│  ── Нові (не призначені) ────────────────────────  │
│                                                     │
│  🆕 28:FF:D3:E5:EE:FF:00:03         T = 28.7°C    │
│     [Оберіть роль ▼]              [💾 Призначити]  │
│                                                     │
│  ── Вільні ролі: condenser_temp, product_temp ──   │
│                                                     │
│  [🔍 Сканувати шину]                                │
│                                                     │
│  ⚠️ Bindings змінено. [🔄 Перезавантажити]         │
└─────────────────────────────────────────────────────┘
```

**Компоненти:**

1. `OneWirePage.svelte` (або секція в Settings)
2. Кнопка "Сканувати" → `fetch('/api/onewire/scan?bus=ow_1')`
3. Список знайдених датчиків з temperature preview
4. Dropdown ролей — тільки вільні (ті що ще не зайняті в bindings)
5. Кнопка "Призначити" → `POST /api/onewire/assign`
6. Банер "Restart needed" якщо bindings змінились

**Доступні ролі:** Беруться з маніфестів equipment модуля — секція `requires_roles`
де type="sensor". Мінус ті що вже зайняті в bindings. 
Endpoint GET /api/ui вже повертає цю інформацію в ui.json.

**Формат адреси в UI:** Показувати з двокрапками "28:FF:A1:B3:CC:DD:EE:01" 
для читабельності, але зберігати і передавати без — "28FFA1B3CCDDEE01".

---

## Задача 8: bindings.json — збереження і оновлення

### Write bindings

Функція для запису оновленого bindings.json на LittleFS:

```cpp
bool ConfigService::save_bindings(const BindingTable& table) {
    // Серіалізуємо в JSON вручну (jsmn — тільки парсер, не генератор)
    char buf[1024];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos,
        "{\"manifest_version\":1,\"bindings\":[");

    for (size_t i = 0; i < table.bindings.size(); i++) {
        const auto& b = table.bindings[i];
        if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
        pos += snprintf(buf + pos, sizeof(buf) - pos,
            "{\"hardware\":\"%s\",\"driver\":\"%s\",\"role\":\"%s\",\"module\":\"%s\"",
            b.hardware_id.c_str(), b.driver_type.c_str(),
            b.role.c_str(), b.module_name.c_str());
        if (!b.address.empty()) {
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                ",\"address\":\"%s\"", b.address.c_str());
        }
        pos += snprintf(buf + pos, sizeof(buf) - pos, "}");
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, "]}");

    FILE* f = fopen("/data/bindings.json", "w");
    if (!f) return false;
    fwrite(buf, 1, pos, f);
    fclose(f);
    return true;
}
```

### API handler додає binding

```cpp
// POST /api/onewire/assign body: {bus, address, role, module}
// 1. Парсимо JSON
// 2. Перевіряємо: адреса 16 hex, роль не зайнята
// 3. Додаємо в binding_table_:
Binding new_binding;
new_binding.hardware_id = bus;        // "ow_1"
new_binding.driver_type = "ds18b20";
new_binding.role = role;              // "condenser_temp"  
new_binding.module_name = module;     // "equipment"
new_binding.address = address;        // "28FFD3E5EEFF0003"
config_service.binding_table().bindings.push_back(new_binding);
// 4. Зберігаємо
config_service.save_bindings(config_service.binding_table());
// 5. restart_needed_ = true
```

---

## Порядок реалізації

1. **hal_types.h** — додати `address` в Binding struct
2. **config_service.cpp** — парсити "address" з JSON + save_bindings()
3. **ds18b20_driver.h/.cpp** — configure(address), write_address(), scan_bus(), read_temp_by_address()
4. **driver_manager.cpp** — передати address в configure()
5. **http_handlers.cpp** — GET /api/onewire/scan, POST /api/onewire/assign, POST /api/onewire/unassign
6. **WebUI** — OneWire page (scan, assign, unassign, restart banner)
7. **Тести** — bindings з address парсяться, scan endpoint, assign/unassign

## Валідація / тестування

- Один датчик без address → SKIP_ROM → працює як раніше ✅
- Два датчики з address на одній шині → MATCH_ROM → кожен читає своє ✅
- Scan → знаходить всі датчики з температурою ✅
- Assign → bindings.json оновлений → після restart датчик працює ✅
- Unassign → binding видалений ✅
- Невідомий address → scan показує як "new" ✅
- Датчик зникає з шини → scan показує ⚠️ offline (assigned але не відповідає) ✅

## Обмеження Phase 11b

- Максимум 8 датчиків на шину (MAX_DEVICES_PER_BUS = 8)
- Потребує restart після assign/unassign (hot-reload — Phase 12+)
- Тільки DS18B20 (інші OneWire пристрої — не підтримуються)
- Conversion optimization (один SKIP_ROM convert для всіх) — TODO, не в цій фазі

---

## Очікуваний результат

| Що | Деталі |
|----|--------|
| C++ зміни | ds18b20_driver (~100 рядків scan + match_rom), hal_types (1 поле), config_service (15 рядків parse + 30 рядків save), driver_manager (3 рядки), http_handlers (~80 рядків endpoints) |
| WebUI | OneWirePage.svelte (~150 рядків) |
| JSON | bindings.json format extension (address field) |
| Backward compat | 100% — без address працює як раніше |

---

## Changelog
- 2026-02-21 — Створено. Phase 11b: OneWire Multi-Sensor з auto-learn.
