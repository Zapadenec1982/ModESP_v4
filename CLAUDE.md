# ModESP v4 — Claude Code Instructions

## Що це за проект

ModESP v4 — модульний фреймворк для промислових ESP32-контролерів (холодильне обладнання).
Замінює дорогі контролери Danfoss/Dixell дешевим ESP32 з професійною архітектурою.

**Цільове залізо:** ESP32-WROOM-32, 4MB flash, ESP-IDF v5.5
**Мова:** C++17 з ETL (Embedded Template Library) замість STL
**Build:** `idf.py build` / `idf.py flash monitor` (COM15)

## Архітектура (критично!)

### Single Source of Truth
```
module manifest.json ─┐
driver manifest.json ─┼→ Python generator → ui.json + C++ headers
board.json + bindings.json ─┘
```
НЕ РЕДАГУЙ ЗГЕНЕРОВАНІ ФАЙЛИ вручну. Вони перезаписуються при кожному build.

### Згенеровані файли (НЕ ЧІПАЙ)
- `data/ui.json` — UI schema для GET /api/ui
- `generated/state_meta.h` — constexpr metadata
- `generated/mqtt_topics.h` — MQTT topic arrays
- `generated/display_screens.h` — LCD/OLED data

### Статичні файли WebUI (можна редагувати вручну)
- `data/www/index.html` — HTML shell (підключає /style.css + /app.js)
- `data/www/app.js` — Runtime renderer (завантажує ui.json через GET /api/ui)
- `data/www/style.css` — Стилі WebUI

### Файли які МОЖНА редагувати
- `modules/*/manifest.json` — опис модулів (UI, state, mqtt, display)
- `drivers/*/manifest.json` — опис драйверів (category, hardware_type, settings)
- `data/board.json` — PCB pin assignment (gpio_outputs, onewire_buses)
- `data/bindings.json` — Runtime: role → driver → GPIO mapping
- `tools/generate_ui.py` — генератор (~900 рядків, генерує 4 артефакти)
- `components/*/src/*.cpp` — C++ реалізація
- `main/main.cpp` — boot sequence та main loop

## Залізні правила коду

### Zero Heap в Hot Path
- НІКОЛИ: std::string, std::vector, new, malloc в on_update() / on_message()
- ЗАВЖДИ: etl::string<N>, etl::vector<T,N>, etl::variant, etl::optional
- SharedState: etl::unordered_map<StateKey, StateValue, 64>
- StateKey = etl::string<24>, StateValue = etl::variant<int32_t, float, bool, etl::string<32>>

### ESP-IDF стиль
- Логування: ESP_LOGI/W/E/D(TAG, ...) — НЕ printf/cout
- static const char* TAG = "ModuleName"; в кожному .cpp
- Коментарі — УКРАЇНСЬКОЮ
- Doxygen headers — англійською
- Build з -Werror=all: кожен warning = помилка компіляції

### Модульна система
- Кожен модуль extends BaseModule (on_init, on_update, on_stop, on_message)
- Кожен модуль — окремий ESP-IDF component з CMakeLists.txt
- Пріоритети: CRITICAL(0) → HIGH(1) → NORMAL(2) → LOW(3)
- init_all() ідемпотентний: пропускає modules з state != CREATED
- Три фази init в main.cpp (system services → WiFi+business → HTTP+WS)

### Message Bus
- etl::message_bus для publish/subscribe між модулями
- Message ID діапазони: System 0-49, Services 50-99, HAL 100-109, Drivers 110-149, Modules 150-249
- Визначення повідомлень — поруч з модулем (*_messages.h)

### State keys формат
- `<module>.<key>` — наприклад: thermostat.temperature, system.uptime
- readwrite float/int — ОБОВ'ЯЗКОВО мають min, max, step

## Структура проекту

```
ModESP_v4/
├── main/main.cpp              # Boot sequence, main loop
├── components/
│   ├── modesp_core/           # BaseModule, ModuleManager, SharedState, types.h
│   ├── modesp_services/       # ErrorService, WatchdogService, Logger, Config, NVS, PersistService
│   ├── modesp_hal/            # HAL, DriverManager, driver interfaces
│   ├── modesp_net/            # WiFiService, HttpService, WsService
│   ├── modesp_json/           # JSON helpers
│   └── jsmn/                  # Lightweight JSON parser (header-only)
├── drivers/
│   ├── ds18b20/
│   │   ├── manifest.json      # ⭐ Driver manifest (sensor, onewire_bus, 3 settings)
│   │   └── src/...            # Dallas DS18B20 temperature sensor (OneWire)
│   └── relay/
│       ├── manifest.json      # ⭐ Driver manifest (actuator, gpio_output, 2 settings)
│       └── src/...            # GPIO relay with min on/off time protection
├── modules/
│   └── thermostat/
│       ├── manifest.json      # ⭐ Single Source of Truth для UI/state/mqtt
│       └── src/thermostat_module.cpp
├── tools/
│   └── generate_ui.py         # Manifest → UI + C++ headers generator
├── data/
│   ├── board.json             # PCB pin assignment (gpio_outputs, onewire_buses)
│   ├── bindings.json          # Runtime: role → driver → GPIO mapping (manifest_version: 1)
│   ├── ui.json                # 🔄 Generated
│   └── www/                   # Static WebUI (index.html, app.js, style.css)
├── generated/                 # 🔄 All generated C++ headers
├── docs/                      # Architecture docs (01-09)
├── project.json               # Active modules list
├── partitions.csv             # NVS(24K) + app(1.5MB) + data/LittleFS(384K)
└── CMakeLists.txt             # Auto-runs generate_ui.py before build
```

## Ключові компоненти

### SharedState — центральне сховище
- etl::unordered_map<StateKey, StateValue, 64>
- Thread-safe (FreeRTOS mutex)
- version_ counter інкрементується на кожен set() — WsService порівнює версії
- for_each() з callback під mutex для серіалізації
- **Persist callback:** set() перевіряє зміну значення → викликає callback ПОЗА mutex
- set_persist_callback(cb, user_data) — реєстрація PersistService як слухача

### PersistService — auto-persist settings в NVS
- Пріоритет CRITICAL — ініціалізується в Phase 1 (до бізнес-модулів)
- **Boot:** ітерує STATE_META, для persist:true читає NVS → SharedState (або default)
- **Runtime:** SharedState callback → dirty flag → debounce 5s → NVS flush
- NVS namespace: "persist", ключі: "p0", "p1", ... (індекс в STATE_META)
- Підтримує float, int32_t, bool типи
- set_state(SharedState*) — ін'єкція залежності з main.cpp

### ModuleManager — lifecycle
- register_module() → init_all() → update_all() в main loop → stop_all()
- sort_by_priority() при init
- restart_module() для WatchdogService recovery
- etl::message_bus<24> для pub/sub

### HTTP API (port 80)
| Endpoint | Method | Description |
|----------|--------|-------------|
| /api/state | GET | Весь SharedState як JSON |
| /api/ui | GET | UI schema (ui.json) |
| /api/board | GET | Board config |
| /api/bindings | GET | Driver bindings |
| /api/modules | GET | Module list + status |
| /api/settings | POST | Зміна readwrite state keys (валідація через state_meta: writable, min/max clamp) |
| /api/mqtt | GET | MQTT config + status JSON |
| /api/mqtt | POST | MQTT config update → NVS + reconnect |
| /api/wifi | POST | WiFi credentials |
| /api/wifi/scan | GET | WiFi scan results |
| /api/ota | GET | Firmware version/partition info |
| /api/ota | POST | OTA firmware upload (.bin) |
| /api/restart | POST | ESP restart |
| /ws | WS | Real-time state broadcast |
| /* | GET | Static files (LittleFS) |

### WebSocket
- Broadcast state JSON при зміні version counter
- Heartbeat PING кожні 30s
- Manual control frames (PING/PONG/CLOSE)
- Max 4 concurrent clients

### WiFi
- STA mode: підключення до існуючого роутера (credentials з NVS)
- AP fallback: ModESP-XXXX (якщо STA не вдалося)
- IP: залежить від режиму (STA = DHCP, AP = 192.168.4.1)

---

## Правила документування (ОБОВ'ЯЗКОВО)

### Після кожної сесії Claude Code ПОВИНЕН:

1. **Оновити цей файл (CLAUDE.md)** якщо змінилась архітектура, API, структура проекту, або поведінка системи
2. **Оновити ACTION_PLAN.md** — зняти галочки з завершених задач, додати нові якщо виникли
3. **Оновити docs/06_roadmap.md** — якщо завершена фаза або змінились пріоритети
4. **Записати відомі баги/TODO** в ACTION_PLAN.md або створити issue

### Принцип: документація = дзеркало коду

- Документація ЗАВЖДИ відображає ПОТОЧНИЙ стан коду, не плани
- Якщо фіча працює — вона описана. Якщо не працює — не описана як готова
- Roadmap описує МАЙБУТНЄ, CLAUDE.md описує СЬОГОДЕННЯ
- Не описуй в CLAUDE.md те чого ще немає в коді
- Не залишай в roadmap як "заплановане" те що вже працює

### Формат запису змін

В кінці кожного документа — секція Changelog:
```
## Changelog
- YYYY-MM-DD — Що змінено і чому (1 рядок)
```

### Документація проекту

| Файл | Що описує | Коли оновлювати |
|------|-----------|-----------------|
| `CLAUDE.md` | Як працює проект ЗАРАЗ | При зміні архітектури/API/структури |
| `ACTION_PLAN.md` | Що робити далі | Після кожної сесії |
| `docs/06_roadmap.md` | Куди йдемо (фази 6-12) | При завершенні фази |
| `docs/10_manifest_standard.md` | Стандарт маніфестів | При зміні формату manifest |
| `next_prompt.md` | Промпт для наступної сесії | В кінці поточної сесії |

## Changelog
- 2026-02-17 — Phase 6.5 DONE: PersistService (CRITICAL, Phase 1). SharedState persist callback (ПОЗА mutex).
  state_meta.h: persist+default_val. POST /api/settings: state_meta валідація (writable, min/max clamp).
  Thermostat: hardcoded config.setpoint замінено на SharedState read. 79 pytest тестів зелені.
- 2026-02-17 — Inputs validation в generate_ui.py: _validate_inputs() в ManifestValidator, 6 правил з docs/10 §3.2a.
  73 pytest тести зелені. Thermostat без inputs (єдиний модуль) — працює як раніше.
- 2026-02-17 — Phase 6 DONE: MQTT WebUI page додана (generate_ui.py + app.js). mqtt.broker в SharedState.
  WiFi PS bug виправлений (DS18B20 esp_wifi_set_ps видалений). Thermostat: temperature→SharedState,
  settings sync, gauge→value. OTA + MQTT endpoints додані в HTTP API таблицю. Milestone M2 ДОСЯГНУТО.
- 2026-02-17 — Driver manifests (ds18b20, relay) + DriverManifestValidator + cross-валідація module↔driver.
  board.json: relays→gpio_outputs. C++ HAL оновлений (BoardConfig.gpio_outputs). Bindings page в WebUI.
  generate_ui.py: ~900 рядків, --drivers-dir, 66 тестів зелені.
- 2026-02-17 — Видалено HTMLGenerator з generate_ui.py (820→755 рядків, 4 артефакти замість 5). WebUI тепер статичний (data/www/)
- 2026-02-17 — Додано правила документування. WiFi: виправлено (STA працює, не тільки AP)
- 2026-02-16 — Створено
