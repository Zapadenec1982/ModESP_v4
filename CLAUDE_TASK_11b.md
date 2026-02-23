# CLAUDE_TASK: Phase 11b — OneWire Multi-Sensor (Auto-learn)

> Підтримка кількох DS18B20 на одній OneWire шині з авто-пошуком через WebUI.
> Scan → показати нові датчики з температурою → призначити роль → зберегти в bindings.json.

---

## Статус виконання

| Задача | Опис | Статус |
|--------|------|--------|
| 1. Binding.address | hal_types.h: SensorAddress + address поле | DONE |
| 2. config_service parse | Парсинг "address" з bindings.json | DONE |
| 3. DS18B20 MATCH_ROM | send_rom_command(), parse_address() + CRC8 | DONE |
| 4. DriverManager | Передача address в ds18b20 configure() | DONE |
| 5. POST /api/bindings | Збереження bindings через BindingsEditor WebUI | DONE |
| 6. SEARCH_ROM | Алгоритм пошуку пристроїв на шині (Maxim AN187) | DONE |
| 7. HTTP scan endpoint | GET /api/onewire/scan?bus=ow_1 | DONE |
| 8. WebUI Discovery | Кнопка "Scan" в BindingsEditor + preview температури | DONE |

---

## Контекст

DS18B20 драйвер вже підтримує **MATCH_ROM** з 64-bit ROM адресою — кожен датчик на шині
адресується індивідуально. Адреса вказується в bindings.json у форматі `"28:FF:AA:BB:CC:DD:EE:01"`
і парситься з CRC8 валідацією. Backward compatible: без address → SKIP_ROM.

**Що вже працює:**
- `Binding.address` (SensorAddress = etl::string<24>) в hal_types.h
- `config_service.cpp` парсить "address" з bindings.json
- `DS18B20Driver::configure()` приймає address, `parse_address()` парсить "28:FF:..." → 8 байт + CRC8
- `send_rom_command()` обирає MATCH_ROM або SKIP_ROM залежно від has_address_
- `DriverManager::create_sensor()` передає address в configure()
- `BindingsEditor.svelte` — повноцінний UI для sensors/actuators з полем address (input з placeholder)
- `POST /api/bindings` — зберігає bindings.json на LittleFS, повертає `{needs_restart: true}`

**Що залишилось:**
1. **SEARCH_ROM** — алгоритм пошуку всіх пристроїв на OneWire шині
2. **HTTP endpoint** — GET /api/onewire/scan для виявлення датчиків з live температурою
3. **WebUI** — кнопка "Scan bus" в BindingsEditor з preview і авто-заповненням address

---

## Існуюча кодова база (reference)

### DS18B20 Driver (`drivers/ds18b20/`)

```
ds18b20_driver.h:
  - configure(role, gpio, interval, address)   // address = "28:FF:AA:BB:..."
  - rom_address_[8], has_address_
  - send_rom_command()                         // MATCH_ROM або SKIP_ROM
  - parse_address(addr_str) → bool             // "28:FF:..." → bytes + CRC8
  - crc8(data, len)                            // Dallas/Maxim CRC8 (static)
  - onewire_reset/write_byte/read_byte/write_bit/read_bit  // bit-bang, instance methods

ds18b20_driver.cpp:
  - OneWire bit-bang з portDISABLE_INTERRUPTS (critical sections ~560µs per byte)
  - Таймінги: reset=480µs, write-1=6µs LOW + 64µs HIGH, write-0=60µs LOW + 10µs
  - read_bit: 3µs LOW, 9µs release, sample, 56µs recovery
```

### BindingsEditor (`webui/src/pages/BindingsEditor.svelte`)

```
- apiGet('/api/bindings') → bindings[]
- apiPost('/api/bindings', {manifest_version:1, bindings}) → {needs_restart}
- roles[] та hwInventory[] з ui.json bindings page
- Поле address: <input class="addr-input" placeholder="28:FF:AA:BB:CC:DD:EE:01">
- addRole() додає binding з address: '' якщо roleDef.requires_address
- setAddress(role, addr) оновлює address для ролі
- Банер "Restart required" + кнопка restart
```

### HTTP endpoints (вже є)

```
GET  /api/bindings   → bindings.json contents
POST /api/bindings   → save bindings.json (LittleFS), returns {needs_restart: true}
POST /api/restart    → esp_restart()
```

---

## Задача 6: SEARCH_ROM алгоритм

**Файли:** `drivers/ds18b20/include/ds18b20_driver.h`, `drivers/ds18b20/src/ds18b20_driver.cpp`

### 6a. Нові public методи в ds18b20_driver.h

```cpp
    // ── Scan API (статичні — не потребують instance) ──
    struct RomAddress {
        uint8_t bytes[8];   // family + serial + CRC
    };
    static constexpr size_t MAX_DEVICES_PER_BUS = 8;

    /// Сканує OneWire шину, повертає кількість знайдених пристроїв
    static size_t scan_bus(gpio_num_t gpio, RomAddress* results, size_t max_results);

    /// Читає температуру з конкретного датчика по адресі (для preview)
    static bool read_temp_by_address(gpio_num_t gpio, const RomAddress& addr, float& temp_out);

    /// Форматує адресу в "28:FF:AA:BB:CC:DD:EE:01"
    static void format_address(const uint8_t* addr, char* out, size_t out_size);
```

### 6b. Нові static private helpers

OneWire low-level методи в існуючому коді — instance methods (onewire_write_byte і т.д.).
Для static scan_bus потрібні **static версії** цих функцій:

```cpp
private:
    // Static OneWire helpers для scan (без instance)
    static bool     ow_reset(gpio_num_t gpio);
    static void     ow_write_byte(gpio_num_t gpio, uint8_t data);
    static uint8_t  ow_read_byte(gpio_num_t gpio);
    static void     ow_write_bit(gpio_num_t gpio, uint8_t bit);
    static uint8_t  ow_read_bit(gpio_num_t gpio);
```

Реалізація — дублікат існуючих instance методів але з gpio параметром замість gpio_.
Альтернатива: зробити існуючі методи static і передавати gpio — але це ламає API,
тому безпечніше мати окремий set.

### 6c. SEARCH_ROM реалізація (Maxim AN187)

```cpp
size_t DS18B20Driver::scan_bus(gpio_num_t gpio, RomAddress* results, size_t max_results) {
    // Налаштувати GPIO для OneWire (open-drain + pull-up)
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << gpio);
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(gpio, 1);

    size_t found = 0;
    uint8_t last_discrepancy = 0;    // Bit position of last conflict (1-based)
    bool last_device = false;
    uint8_t prev_rom[8] = {};        // ROM з попереднього проходу

    while (!last_device && found < max_results) {
        // 1. Reset bus
        if (!ow_reset(gpio)) break;  // Ніхто не відповів

        // 2. Send SEARCH ROM command (0xF0)
        ow_write_byte(gpio, 0xF0);

        uint8_t rom[8] = {};
        uint8_t new_discrepancy = 0;  // Нова позиція конфлікту

        // 3. 64-bit search loop
        for (uint8_t bit_pos = 1; bit_pos <= 64; bit_pos++) {
            // Індекси для доступу до rom[]
            uint8_t byte_idx = (bit_pos - 1) / 8;
            uint8_t bit_mask = 1 << ((bit_pos - 1) % 8);

            // Read bit and its complement
            uint8_t bit_val    = ow_read_bit(gpio);
            uint8_t bit_comp   = ow_read_bit(gpio);

            if (bit_val == 1 && bit_comp == 1) {
                // Ніхто не відповів (помилка або пристрій зник)
                break;
            }

            uint8_t direction;

            if (bit_val != bit_comp) {
                // Всі пристрої мають однаковий біт — немає конфлікту
                direction = bit_val;
            } else {
                // Конфлікт: bit=0, comp=0 → є пристрої і з 0, і з 1
                if (bit_pos == last_discrepancy) {
                    // Цього разу йдемо по 1 (минулого разу йшли по 0)
                    direction = 1;
                } else if (bit_pos > last_discrepancy) {
                    // Нова позиція — йдемо по 0, запам'ятовуємо
                    direction = 0;
                } else {
                    // Повторюємо вибір з попереднього проходу
                    direction = (prev_rom[byte_idx] & bit_mask) ? 1 : 0;
                }

                if (direction == 0) {
                    // Запам'ятати цю позицію як останній конфлікт
                    new_discrepancy = bit_pos;
                }
            }

            // Записуємо напрямок в ROM
            if (direction) {
                rom[byte_idx] |= bit_mask;
            }

            // Повідомляємо шину обраний напрямок
            ow_write_bit(gpio, direction);
        }

        // 4. CRC8 перевірка знайденої адреси
        if (crc8(rom, 7) == rom[7]) {
            memcpy(results[found].bytes, rom, 8);
            found++;
            ESP_LOGI(TAG, "Found device: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                     rom[0], rom[1], rom[2], rom[3],
                     rom[4], rom[5], rom[6], rom[7]);
        } else {
            ESP_LOGW(TAG, "CRC mismatch during bus scan — skipping device");
        }

        // 5. Зберігаємо для наступного проходу
        memcpy(prev_rom, rom, 8);
        last_discrepancy = new_discrepancy;

        if (last_discrepancy == 0) {
            last_device = true;  // Всі гілки дерева пройдені
        }
    }

    ESP_LOGI(TAG, "Bus scan complete: %d device(s) on GPIO %d", (int)found, gpio);
    return found;
}
```

### 6d. read_temp_by_address()

Для preview під час сканування — запускає conversion і читає температуру з конкретного датчика:

```cpp
bool DS18B20Driver::read_temp_by_address(gpio_num_t gpio,
                                          const RomAddress& addr, float& temp_out) {
    // Start conversion
    if (!ow_reset(gpio)) return false;
    ow_write_byte(gpio, CMD_MATCH_ROM);
    for (int i = 0; i < 8; i++) ow_write_byte(gpio, addr.bytes[i]);
    ow_write_byte(gpio, CMD_CONVERT_T);

    // Чекаємо 750ms на конвертацію
    vTaskDelay(pdMS_TO_TICKS(800));

    // Read scratchpad
    if (!ow_reset(gpio)) return false;
    ow_write_byte(gpio, CMD_MATCH_ROM);
    for (int i = 0; i < 8; i++) ow_write_byte(gpio, addr.bytes[i]);
    ow_write_byte(gpio, CMD_READ_SCRATCH);

    uint8_t scratchpad[9];
    for (int i = 0; i < 9; i++) scratchpad[i] = ow_read_byte(gpio);

    // CRC8 scratchpad check
    if (crc8(scratchpad, 8) != scratchpad[8]) return false;

    // Decode temperature
    int16_t raw = (static_cast<int16_t>(scratchpad[1]) << 8) | scratchpad[0];
    if (raw == 0x0550) return false;  // Power-on reset (85°C)

    temp_out = raw / 16.0f;
    return true;
}
```

### 6e. format_address()

```cpp
void DS18B20Driver::format_address(const uint8_t* addr, char* out, size_t out_size) {
    snprintf(out, out_size, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
             addr[0], addr[1], addr[2], addr[3],
             addr[4], addr[5], addr[6], addr[7]);
}
```

**Важливі нюанси:**

1. **GPIO конфлікт:** scan_bus() налаштовує GPIO, який вже використовується працюючим DS18B20Driver.
   Це безпечно — gpio_config() ідемпотентний, і scan виконується з httpd таска, тоді як
   driver update з main loop. Але conversion може конфліктувати. Рішення: scan блокуючий
   (~800ms на датчик), виконується рідко (по натисканню кнопки), вимикати scan при активних
   драйверах не потрібно (worst case — один read fail з retry).

2. **Час виконання:** scan_bus() + read_temp_by_address() для N датчиків ≈ N × 900ms.
   Для 4 датчиків ~3.6 секунди. HTTP handler повинен мати достатній timeout.

3. **Формат адреси:** Всюди (bindings.json, API, UI) використовується формат з двокрапками:
   `"28:FF:AA:BB:CC:DD:EE:01"`. parse_address() вже підтримує і `:`, і `-`, і без роздільників.

---

## Задача 7: HTTP endpoint для scan

**Файл:** `components/modesp_net/src/http_service.cpp` (або окремий файл)

### 7a. GET /api/onewire/scan?bus=ow_1

Сканує шину, повертає знайдені датчики з температурою і статусом:

```json
{
  "bus": "ow_1",
  "gpio": 15,
  "devices": [
    {
      "address": "28:FF:A1:B3:CC:DD:EE:01",
      "temperature": 4.5,
      "role": "air_temp",
      "status": "assigned"
    },
    {
      "address": "28:FF:B2:C4:DD:EE:AA:02",
      "temperature": -18.3,
      "role": null,
      "status": "new"
    }
  ]
}
```

**Алгоритм handler:**

```cpp
esp_err_t HttpService::handle_get_ow_scan(httpd_req_t* req) {
    auto* self = static_cast<HttpService*>(req->user_ctx);
    set_cors_headers(req);

    // 1. Отримати bus_id з query string
    char bus_id[16] = {};
    if (httpd_req_get_url_query_str(req, ...) != ESP_OK) { ... }
    // Parse "bus=ow_1" → bus_id = "ow_1"

    // 2. Знайти GPIO для шини через HAL
    auto* ow_res = self->hal_->find_onewire_bus(bus_id);
    if (!ow_res) { return httpd_resp_send_err(req, 404, "Bus not found"); }

    // 3. Scan bus
    DS18B20Driver::RomAddress devices[DS18B20Driver::MAX_DEVICES_PER_BUS];
    size_t count = DS18B20Driver::scan_bus(ow_res->gpio, devices,
                                            DS18B20Driver::MAX_DEVICES_PER_BUS);

    // 4. Для кожного — прочитати температуру + перевірити bindings
    char json[1024];
    int pos = snprintf(json, sizeof(json),
        "{\"bus\":\"%s\",\"gpio\":%d,\"devices\":[", bus_id, ow_res->gpio);

    for (size_t i = 0; i < count; i++) {
        char addr_str[24];
        DS18B20Driver::format_address(devices[i].bytes, addr_str, sizeof(addr_str));

        float temp = 0;
        bool has_temp = DS18B20Driver::read_temp_by_address(ow_res->gpio, devices[i], temp);

        // Перевірити чи є в bindings
        const char* role = nullptr;
        const char* status = "new";
        // ... пошук по config_service binding_table

        if (i > 0) pos += snprintf(json + pos, sizeof(json) - pos, ",");
        pos += snprintf(json + pos, sizeof(json) - pos,
            "{\"address\":\"%s\"", addr_str);
        if (has_temp) {
            pos += snprintf(json + pos, sizeof(json) - pos,
                ",\"temperature\":%.1f", temp);
        }
        if (role) {
            pos += snprintf(json + pos, sizeof(json) - pos,
                ",\"role\":\"%s\",\"status\":\"assigned\"", role);
        } else {
            pos += snprintf(json + pos, sizeof(json) - pos,
                ",\"role\":null,\"status\":\"new\"");
        }
        pos += snprintf(json + pos, sizeof(json) - pos, "}");
    }
    pos += snprintf(json + pos, sizeof(json) - pos, "]}");

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, pos);
}
```

### 7b. Реєстрація endpoint

```cpp
{"/api/onewire/scan", HTTP_GET, handle_get_ow_scan},
```

**Не потрібні окремі POST /api/onewire/assign і /api/onewire/unassign!**
Вже є `POST /api/bindings` який зберігає весь масив bindings.
WebUI просто оновлює bindings масив локально і зберігає через існуючий endpoint.

### 7c. Залежності

HttpService потребує доступ до:
- `HAL*` — для find_onewire_bus() (GPIO по bus_id)
- `ConfigService*` (або BindingTable) — для порівняння знайдених адрес з assigned

Варіант: передати HAL* через set_hal() (аналогічно до set_state(), set_wifi()).

---

## Задача 8: WebUI — Discovery в BindingsEditor

**Файл:** `webui/src/pages/BindingsEditor.svelte`

### 8a. Додати секцію OneWire Discovery

Нова секція Card під існуючими Sensors/Actuators картками:

```svelte
<Card title="OneWire Discovery">
  <div class="scan-controls">
    <select bind:value={selectedBus}>
      {#each owBuses as bus}
        <option value={bus.id}>{bus.id} (GPIO {bus.gpio})</option>
      {/each}
    </select>
    <button class="scan-btn" on:click={scanBus} disabled={scanning}>
      {scanning ? 'Scanning...' : 'Scan Bus'}
    </button>
  </div>

  {#if scanResults.length > 0}
    {#each scanResults as device}
      <div class="device-row" class:assigned={device.status === 'assigned'}>
        <span class="device-addr">{device.address}</span>
        {#if device.temperature !== undefined}
          <span class="device-temp">{device.temperature.toFixed(1)} C</span>
        {/if}
        {#if device.status === 'assigned'}
          <span class="device-role">{device.role}</span>
        {:else}
          <select bind:value={device.selectedRole}>
            <option value="">-- Select role --</option>
            {#each freeRoles as role}
              <option value={role.role}>{role.label}</option>
            {/each}
          </select>
          <button class="assign-btn"
                  disabled={!device.selectedRole}
                  on:click={() => assignDevice(device)}>
            Assign
          </button>
        {/if}
      </div>
    {/each}
  {/if}
</Card>
```

### 8b. JavaScript логіка

```javascript
let scanning = false;
let scanResults = [];
let selectedBus = 'ow_1';

// OneWire шини з board info (hwInventory або окремий запит)
$: owBuses = hwInventory.filter(h => h.hw_type === 'onewire_bus');

// Вільні сенсорні ролі (requires_address=true, ще не assigned)
$: freeRoles = roles
    .filter(r => r.requires_address && !assignedRoles.has(r.role));

async function scanBus() {
    scanning = true;
    try {
        const data = await apiGet(`/api/onewire/scan?bus=${selectedBus}`);
        scanResults = (data.devices || []).map(d => ({...d, selectedRole: ''}));
    } catch (e) {
        error = e.message;
    } finally {
        scanning = false;
    }
}

function assignDevice(device) {
    // Додаємо новий binding в масив
    bindings = [...bindings, {
        hardware: selectedBus,
        driver: 'ds18b20',
        role: device.selectedRole,
        module: 'equipment',
        address: device.address,
    }];
    // Оновлюємо scan results
    device.status = 'assigned';
    device.role = device.selectedRole;
    scanResults = [...scanResults];
}
```

### 8c. UX flow

```
1. Користувач відкриває Bindings → бачить поточні sensors/actuators
2. В секції "OneWire Discovery" вибирає шину → натискає "Scan Bus"
3. Очікує 3-5 сек (scan + temperature read)
4. Бачить список знайдених датчиків:
   - Вже assigned: "28:FF:AA:BB:...  4.5°C  air_temp"
   - Нові: "28:FF:DD:EE:...  28.7°C  [Select role ▼] [Assign]"
5. Вибирає роль → "Assign" → датчик з'являється в Sensors секції
6. Натискає Save → POST /api/bindings
7. Банер "Restart required" → натискає Restart
```

**Альтернативний UX (простіший):**
Замість окремої discovery секції — додати кнопку "Scan" поруч з полем address.
Відкривається popup/dropdown зі знайденими адресами + температурами.
Натискаєш на адресу → вона вставляється в поле. Менше коду, працює в існуючому layout.

---

## Порядок реалізації залишку

1. **ds18b20_driver.h/.cpp** — static OneWire helpers + scan_bus() + read_temp_by_address() + format_address()
2. **http_service** — GET /api/onewire/scan (потребує HAL injection)
3. **BindingsEditor.svelte** — scan UI секція
4. **Тести** — pytest для scan endpoint response format

---

## Валідація / тестування

### Вже перевірено (Tasks 1-5):
- [x] Один датчик без address → SKIP_ROM → працює як раніше
- [x] Binding з address парситься через config_service
- [x] DS18B20 MATCH_ROM: send_rom_command() обирає правильну команду
- [x] parse_address() парсить "28:FF:..." з CRC8 валідацією
- [x] DriverManager передає address в configure()
- [x] BindingsEditor показує поле address для requires_address ролей
- [x] POST /api/bindings зберігає bindings.json з address полем

### Потребує тестування (Tasks 6-8):
- [ ] scan_bus() знаходить всі DS18B20 на шині (HIL test з 2+ датчиками)
- [ ] SEARCH_ROM коректно обходить дерево при конфліктах
- [ ] CRC8 відсіює невалідні адреси під час scan
- [ ] read_temp_by_address() повертає коректну температуру
- [ ] GET /api/onewire/scan повертає JSON з devices + temperature + status
- [ ] Assigned датчики показуються з role, нові — з status "new"
- [ ] WebUI scan button працює, assign додає binding
- [ ] Save + Restart → новий датчик працює з MATCH_ROM

---

## Обмеження

- Максимум 8 датчиків на шину (MAX_DEVICES_PER_BUS = 8)
- Scan блокуючий: ~900ms на датчик (conversion + read). 4 датчики ≈ 3.6 сек
- Потребує restart після зміни bindings (hot-reload — складний, не в цій фазі)
- Тільки DS18B20 (інші OneWire пристрої — ігноруються scan)
- Conversion optimization (один SKIP_ROM convert для всіх) — TODO, не в цій фазі
- Scan з httpd таска може конфліктувати з main loop driver reads (worst case — retry)

---

## Очікуваний обсяг залишку

| Файл | Зміни |
|------|-------|
| `drivers/ds18b20/include/ds18b20_driver.h` | +15 рядків (static methods + RomAddress struct) |
| `drivers/ds18b20/src/ds18b20_driver.cpp` | +120 рядків (static ow_*, scan_bus, read_temp_by_address, format_address) |
| `components/modesp_net/src/http_service.cpp` | +50 рядків (handle_get_ow_scan) |
| `components/modesp_net/include/modesp/net/http_service.h` | +3 рядки (handler decl + HAL*) |
| `webui/src/pages/BindingsEditor.svelte` | +60 рядків (scan UI section) |
| **Разом** | **~250 рядків** |

---

## Changelog
- 2026-02-22 — Оновлено: Tasks 1-5 DONE, виправлено формат адреси (":"-separated per actual code),
  повна реалізація SEARCH_ROM (Maxim AN187), спрощено — не потрібні окремі assign/unassign endpoints
  (використовується існуючий POST /api/bindings). Детальний reference існуючого коду.
- 2026-02-21 — Створено. Phase 11b: OneWire Multi-Sensor з auto-learn.
