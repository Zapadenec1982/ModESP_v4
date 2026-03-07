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
- `generated/features_config.h` — constexpr feature flags (active/inactive per module)

### Svelte WebUI (webui/ → data/www/)
- `webui/src/` — Svelte 4 source code (App.svelte, stores, components, pages)
- `webui/dist/` — Build output (bundle.js, bundle.css) — gitignored
- `data/www/index.html` — SPA shell (підключає /bundle.css + /bundle.js)
- `data/www/bundle.js.gz` — Svelte app gzipped (~63KB)
- `data/www/bundle.css.gz` — Styles gzipped (~13KB)
- Build: `cd webui && npm run build` → Deploy: `npm run deploy` → data/www/
- **Theme:** Light/Dark toggle (stores/theme.js), CSS custom properties, localStorage + prefers-color-scheme
- **i18n:** UA/EN toggle (stores/i18n.js), ~120 inline keys per language, derived `$t` store
- **Animations:** Svelte transitions (fly/fade/slide/scale), value flash on change, staggered card entrance
- **Responsive accordions:** desktop open by default, mobile (< 768px) collapsed (GroupAccordion)
- **Card icons:** shield, flame, thermometer, database, network per module
- **Premium dark theme:** bento-card dashboard layout, unified color tokens

### Equipment Layer (Phase 9.1)
- **EquipmentModule** (priority=CRITICAL) — єдиний модуль з доступом до HAL drivers
- Читає сенсори → публікує `equipment.air_temp`, `equipment.sensor1_ok`, etc.
- Читає requests від бізнес-модулів: `thermostat.req.compressor`, `defrost.req.*`, `protection.lockout`
- Арбітраж: Protection LOCKOUT > Defrost active > Thermostat
- Інтерлоки: defrost_relay↔компресор ніколи одночасно
- **defrost_relay** — unified relay for both electric heater and hot gas valve (merged from heater+hg_valve)
- **Compressor anti-short-cycle (output-level):** COMP_MIN_OFF_MS=180s, COMP_MIN_ON_MS=120s
  Захищає компресор незалежно від джерела запиту (thermostat/defrost)
- Публікує **фактичний** стан реле через `get_state()` (не бажаний `out_`)
- Relay `min_switch_ms` = 0 для всіх реле крім compressor (180s) — defrost_relay/fan перемикаються миттєво
- **EMA filter:** Exponential Moving Average для температур, alpha = 1/(filter_coeff+1), roundf() до 0.01°C для зменшення SharedState version bumps
- **Temperature rounding:** roundf(T * 100) / 100 — зменшує кількість WS broadcasts
- **Night input:** опціональний дискретний вхід для нічного режиму → `equipment.night_input`
- Бізнес-модулі (Thermostat, Defrost, Protection) працюють ТІЛЬКИ через SharedState
- `data/bindings.json` — всі drivers прив'язані до module "equipment"

### Thermostat v2 (Phase 9.2 + 11a)

- **Асиметричний диференціал:** ON при T >= effective_SP + differential, OFF при T <= effective_SP
- **State machine:** STARTUP → IDLE ↔ COOLING, SAFETY_RUN (при відмові датчика)
- **Захист компресора:** min_on_time (хв), min_off_time (хв), startup_delay (хв)
- **Safety Run:** циклічна робота (safety_run_on / safety_run_off) при sensor1_ok=false
- **Вентилятор випарника:** 3 режими (постійно / з компресором / за T_evap з гістерезисом)
- **Вентилятор конденсатора:** синхронно з компресором + затримка OFF (cond_fan_delay)
- **Night Setback:** 4 режими (0=off, 1=schedule SNTP, 2=DI, 3=manual), effective_sp = setpoint + night_setback
- **Display during defrost:** 3 режими (0=real T, 1=frozen T, 2="-d-" symbol) → `thermostat.display_temp`
- **16 persist параметрів** (11 base + night_setback, night_mode, night_start, night_end, display_defrost)
- Requests: `thermostat.req.compressor`, `thermostat.req.evap_fan`, `thermostat.req.cond_fan`

### Protection (Phase 9.3 + 11a)

- **10 незалежних моніторів аварій:** High Temp, Low Temp, Sensor1, Sensor2, Door + Short Cycle, Rapid Cycle, Continuous Run, Pulldown, Rate-of-Change
- **Delayed alarms:** High temp (high_alarm_delay), Low temp (low_alarm_delay), Door (door_delay) — окремі затримки в хвилинах
- **Instant alarms:** Sensor1, Sensor2 — без затримки
- **Defrost blocking:** High Temp + Rate alarm блокується під час heating-фаз defrost (скидається pending)
- **Post-defrost suppression:** High Temp + Rate alarm блокується на post_defrost_delay хвилин після відтайки
- **Auto-clear:** аварія знімається автоматично при поверненні в норму (якщо manual_reset=false)
- **Manual reset:** `protection.reset_alarms` = true → скидає всі 10 аварій (WebUI/API)
- **Компресорний захист (Phase 17):**
  - CompressorTracker: ring buffer 30 starts, sliding 1h window, duty cycle, short cycle detection
  - Short Cycling: 3 послідовних циклів < min_compressor_run → alarm
  - Rapid Cycling: >max_starts_hour запусків за 1 годину → alarm
  - Continuous Run: безперервна робота > max_continuous_run → alarm (auto-clear при OFF)
  - Pulldown Failure: компресор ON > pulldown_timeout, T не впала > pulldown_min_drop (evap_temp fallback)
  - Rate-of-Change: EWMA lambda=0.3, T росте > max_rise_rate протягом rate_duration → alarm
  - Діагностика (кожні 5 сек): starts_1h, duty%, run_time, last_cycle_run/off, compressor_hours
  - 2 features: compressor_protection (requires_roles: [compressor]), rate_protection (requires_roles: [compressor, air_temp])
- **Alarm code priority:** err1 > rate_rise > high_temp > pulldown > short_cycle > rapid_cycle > low_temp > continuous_run > err2 > door > none
- **15 persist параметрів:** high_limit, low_limit, high_alarm_delay, low_alarm_delay, door_delay, manual_reset, post_defrost_delay + min_compressor_run, max_starts_hour, max_continuous_run, pulldown_timeout, pulldown_min_drop, max_rise_rate, rate_duration, compressor_hours
- **compressor_hours:** float, persist — кумулятивні мотогодини (інкремент 5 сек)
- **protection.lockout = false** завжди (зарезервовано)
- Порядок update: Equipment(0) → **Protection(1)** → Thermostat(2)

### Defrost (Phase 9.4)

- **7-фазна state machine:** IDLE → [STABILIZE → VALVE_OPEN →] ACTIVE → [EQUALIZE →] DRIP → FAD → IDLE
- **3 типи відтайки:** 0=природна (зупинка), 1=тен, 2=гарячий газ (7 фаз)
- **4 ініціації:** таймер (interval з counter_mode=1 реальний/2 компресор), demand (T_evap < demand_temp), комбінований, ручний
- **Завершення:** по T_evap >= end_temp (основна), по таймеру безпеки max_duration, consecutive timeout counter
- **Оптимізація:** skip defrost якщо T_evap > end_temp (випарник чистий)
- **FAD:** Fan After Defrost — компресор+конд.вент ON, вент.вип OFF, завершення по fad_temp або таймеру fad_duration
- **14 persist параметрів** + 2 runtime persist (interval_timer, defrost_count)
- **NVS persistence:** interval_timer і defrost_count зберігаються через PersistService
- Requests: `defrost.req.compressor`, `defrost.req.defrost_relay`, `defrost.req.evap_fan`, `defrost.req.cond_fan`
- EM арбітрує: defrost.active=true → defrost.req.* має пріоритет над thermostat.req.*
- Порядок update: Equipment(0) → Protection(1) → **Thermostat(2) + Defrost(2)**

### Features System (Phase 10.5)

- **Manifest-driven feature flags:** кожен модуль оголошує `features` з `requires_roles`
- **FeatureResolver** (Python): перевіряє `bindings.json` → визначає active/inactive features
- **Select widgets:** state keys з `options` (value+label) замість min/max/step для enum settings
- **Constraints (enum_filter):** фільтрація доступних options залежно від active features
- **disabled + disabled_reason:** widgets для неактивних features показуються як disabled в UI
- **features_config.h:** constexpr масив + `is_feature_active(module, feature)` inline lookup
- **has_feature():** метод BaseModule для runtime guards в C++ модулях
- **Drivers:** ds18b20 (sensor, MATCH_ROM multi-sensor, SEARCH_ROM scan), relay (actuator), digital_input (sensor, GPIO input), ntc (sensor, ADC thermistor), pcf8574_relay (I2C actuator), pcf8574_input (I2C sensor)
- **Validation:** V14 (controls_settings exist), V15 (requires_roles exist), V16 (requires_feature exist), V17 (options int), V18 (options→select), V19 (visible_when format)

### Runtime UI Visibility (Phase 13a)

- **visible_when:** cards та widgets ховаються/показуються на основі state key значень
  - Формат: `{"key": "state.key", "eq": value}` або `"neq"` або `"in": [...]`
  - Маніфести: defrost (hot gas card, end_temp, demand_temp, fad_temp), thermostat (fan/night params), protection (door_delay)
  - `isVisible(vw, $state)` — утиліта в `webui/src/lib/visibility.js`
- **requires_state (per-option disabled):** select options завжди присутні в ui.json, але недоступні якщо hardware відсутнє
  - resolve_constraints() тепер зберігає ВСІ options + додає `requires_state` та `disabled_hint`
  - FEATURE_TO_STATE mapping: feature name → `equipment.has_*` state key
  - SelectWidget перевіряє `$state[opt.requires_state]` реактивно
- **equipment.has_* state keys:** `has_defrost_relay`, `has_cond_fan`, `has_door_contact`, `has_evap_temp`, `has_cond_temp`, `has_night_input`, `has_ntc_driver`, `has_ds18b20_driver`
- **Runtime ланцюг:** Bindings page → Save → Restart → equipment.has_* = true → option enabled / card visible

### DataLogger (Phase 14 + 14b)

- **6-channel dynamic logging:** air (завжди) + evap + cond + setpoint + humidity + reserved
- **TempRecord 16 bytes:** timestamp(4) + int16_t ch[6] (12 bytes) — compile-time ChannelDef table
- **ChannelDef:** id, state_key, enable_key, has_key — per-channel enable/hardware detection
- **TEMP_NO_DATA sentinel:** INT16_MIN (-32768) = канал вимкнений або датчик відсутній
- **LittleFS storage:** temp.bin + events.bin з rotate при перевищенні ліміту
- **RAM buffer:** 16 temp + 32 events, flush кожні 10 хвилин
- **Auto-migration:** старі формати (8/12 bytes) видаляються при boot (st_size % 16 != 0)
- **10 event types:** compressor on/off, defrost start/end, alarm high/low/clear, door open/close, power_on
- **JSON v3:** `{"channels":["air","evap","setpoint"],"temp":[[ts,v0,v1,v2],...],"events":[[ts,type],...]}`
  - Dynamic channels: serialize scans files for non-null data, outputs only active channels
  - Values = null якщо TEMP_NO_DATA; GET /api/log?hours=24 — streaming chunked
- **GET /api/log/summary:** {hours, temp_count, event_count, flash_kb}
- **ChartWidget (SVG):** fully dynamic channels from API response, PALETTE colors per channel,
  toggle checkboxes, downsample (max 720 min/max bucket per channel), zones (comp/defrost), tooltip
- **Setpoint dual-mode:** if logged as channel → polyline; if not → horizontal line from live $state
- **Event list:** `<details>` секція під графіком, останні 50 подій з текстовими мітками
- **CSV export:** client-side download (dynamic channels + events), zero ESP32 навантаження
- **10 state keys** (7 persist: enabled, retention_hours, sample_interval, log_evap, log_cond,
  log_setpoint, log_humidity; 3 readonly: records_count, events_count, flash_used)
- **264 pytest тестів + 90 host C++ doctest**
- **Host C++ tests:** tests/host/ (doctest, thermostat/defrost/protection)

### KC868-A6 Board Support (Phase 12a)
- **I2C bus + PCF8574 expander** в HAL (i2c_buses, i2c_expanders у board.json)
- **pcf8574_relay driver** — relay через I2C PCF8574 (8-bit port expander)
- **pcf8574_input driver** — digital input через I2C PCF8574
- **board_kc868a6.json** — 6 реле (PCF8574 @0x24), 6 входів (PCF8574 @0x22)
- **100% сумісний** з dev board (GPIO) — той же firmware, різний board.json

### Файли які МОЖНА редагувати
- `modules/*/manifest.json` — опис модулів (UI, state, mqtt, display, features, constraints)
- `drivers/*/manifest.json` — опис драйверів (category, hardware_type, settings)
- `data/board.json` — PCB pin assignment (gpio_outputs, onewire_buses, gpio_inputs, adc_channels, i2c_buses, i2c_expanders, expander_outputs, expander_inputs)
- `data/bindings.json` — Runtime: role → driver → GPIO mapping
- `tools/generate_ui.py` — генератор (~1677 рядків, генерує 5 артефактів)
- `components/*/src/*.cpp` — C++ реалізація
- `main/main.cpp` — boot sequence та main loop

## Залізні правила коду

### Zero Heap в Hot Path
- НІКОЛИ: std::string, std::vector, new, malloc в on_update() / on_message()
- ЗАВЖДИ: etl::string<N>, etl::vector<T,N>, etl::variant, etl::optional
- SharedState: etl::unordered_map<StateKey, StateValue, 128> (MODESP_MAX_STATE_ENTRIES)
- StateKey = etl::string<32>, StateValue = etl::variant<int32_t, float, bool, etl::string<32>>

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
- readwrite float/int — ОБОВ'ЯЗКОВО мають min, max, step (або options для enum)

## Маніфести — features та options

### Features (Phase 10.5)
- Кожен бізнес-модуль оголошує `features` з `requires_roles` в маніфесті
- `always_active: true` — feature завжди активна; інакше requires_roles перевіряються проти bindings
- `controls_settings: [...]` — які state keys контролює ця feature
- При генерації: неактивна feature → widgets disabled в ui.json, false в features_config.h
- C++ модулі: `has_feature("name")` → constexpr lookup

### Options (select widgets)
- State key з полем `options: [{value, label}, ...]` → widget "select" в UI
- `constraints` секція: `enum_filter` → requires_feature → FEATURE_TO_STATE → requires_state на option
- Всі options завжди присутні в ui.json; недоступні показуються як disabled з hint
- Приклад: defrost.type "Гарячий газ" має `requires_state: "equipment.has_defrost_relay"` — disabled поки не підключений

## Структура проекту

```
ModESP_v4/
├── main/main.cpp              # Boot sequence, main loop
├── components/
│   ├── modesp_core/           # BaseModule, ModuleManager, SharedState, types.h
│   ├── modesp_services/       # ErrorService, WatchdogService, Logger, Config, NVS, PersistService
│   ├── modesp_hal/            # HAL, DriverManager, driver interfaces
│   ├── modesp_net/            # WiFiService, HttpService, WsService
│   ├── modesp_mqtt/           # MqttService (ESP-MQTT client, auto-reconnect)
│   ├── modesp_json/           # JSON helpers
│   └── jsmn/                  # Lightweight JSON parser (header-only)
├── drivers/
│   ├── digital_input/
│   │   ├── manifest.json      # ⭐ Driver manifest (sensor, gpio_input, 1 setting)
│   │   └── src/...            # GPIO input with 50ms debounce
│   ├── ds18b20/
│   │   ├── manifest.json      # ⭐ Driver manifest (sensor, onewire_bus, 3 settings)
│   │   └── src/...            # Dallas DS18B20 (SKIP_ROM + MATCH_ROM multi-sensor)
│   ├── ntc/
│   │   ├── manifest.json      # ⭐ Driver manifest (sensor, adc_channel, 5 settings)
│   │   └── src/...            # NTC thermistor via ADC (B-parameter equation)
│   ├── relay/
│   │   ├── manifest.json      # ⭐ Driver manifest (actuator, gpio_output, 2 settings)
│   │   └── src/...            # GPIO relay with min on/off time protection
│   ├── pcf8574_relay/
│   │   ├── manifest.json      # ⭐ Driver manifest (actuator, expander_output)
│   │   └── src/...            # I2C PCF8574 relay (8-bit port expander)
│   └── pcf8574_input/
│       ├── manifest.json      # ⭐ Driver manifest (sensor, expander_input)
│       └── src/...            # I2C PCF8574 digital input
├── modules/
│   ├── equipment/
│   │   ├── manifest.json      # ⭐ Equipment Manager (HAL owner, arbitration)
│   │   └── src/equipment_module.cpp
│   ├── protection/
│   │   ├── manifest.json      # ⭐ Alarm monitoring (10 monitors, compressor safety)
│   │   └── src/protection_module.cpp
│   ├── thermostat/
│   │   ├── manifest.json      # ⭐ Single Source of Truth для UI/state/mqtt
│   │   └── src/thermostat_module.cpp
│   ├── defrost/
│   │   ├── manifest.json      # ⭐ 7-phase defrost cycle (3 types, 14 params)
│   │   └── src/defrost_module.cpp
│   └── datalogger/
│       ├── manifest.json      # ⭐ 6-channel dynamic logging + events
│       └── src/datalogger_module.cpp
├── tools/
│   ├── generate_ui.py         # Manifest → UI + C++ headers generator (~1200 lines)
│   └── tests/                 # 264 pytest tests (test_features, test_modules, test_validator)
├── tests/
│   └── host/                  # Host C++ unit tests (doctest, 90 test cases)
├── data/
│   ├── board.json             # Активна плата (зараз KC868-A6: I2C PCF8574 expanders)
│   ├── board_kc868a6.json     # KC868-A6 reference (копія board.json)
│   ├── bindings.json          # Активні bindings (зараз KC868-A6: pcf8574_relay + pcf8574_input)
│   ├── ui.json                # 🔄 Generated
│   └── www/                   # Deployed WebUI (index.html, bundle.js.gz, bundle.css.gz)
├── webui/                     # Svelte 4 WebUI source
│   ├── src/                   # App.svelte, stores/, lib/, components/, pages/
│   ├── scripts/deploy.js      # Gzip + copy to data/www/
│   ├── package.json           # npm run build / npm run deploy
│   └── rollup.config.js       # Rollup bundler config
├── generated/                 # 🔄 All generated C++ headers (5 files)
├── docs/                      # Architecture docs (01-10, CHANGELOG)
├── project.json               # Active modules list
├── partitions.csv             # NVS(24K) + app(1.5MB) + data/LittleFS(384K)
└── CMakeLists.txt             # Auto-runs generate_ui.py before build
```

## Ключові компоненти

### SharedState — центральне сховище
- etl::unordered_map<StateKey, StateValue, 128> (MODESP_MAX_STATE_ENTRIES)
- Thread-safe (FreeRTOS mutex)
- version_ counter інкрементується на кожен set() — WsService порівнює версії
- for_each() з callback під mutex для серіалізації
- **Persist callback:** set() перевіряє зміну значення → викликає callback ПОЗА mutex
- set_persist_callback(cb, user_data) — реєстрація PersistService як слухача

### PersistService — auto-persist settings в NVS
- Пріоритет CRITICAL — ініціалізується в Phase 1 (до бізнес-модулів)
- **Boot:** ітерує STATE_META, для persist:true читає NVS → SharedState (або default)
- **Runtime:** SharedState callback → dirty flag → debounce 5s → NVS flush
- NVS namespace: "persist", ключі: djb2 hash від імені state key ("s" + 7 hex chars)
- **Міграція при boot:** автоматично конвертує старі позиційні ключі "p0".."p32" → hash-based
- Підтримує float, int32_t, bool типи (з auto-conversion між float↔int32_t)
- **NVS batch API:** batch_open/batch_close для зменшення heap фрагментації (один handle для множинних read/write)
- **Flush optimization:** один nvs_open → всі dirty keys → один commit → nvs_close
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
| /api/onewire/scan | GET | Scan OneWire bus (SEARCH_ROM), return devices with T |
| /api/log | GET | DataLogger: streaming chunked JSON (?hours=24) |
| /api/log/summary | GET | DataLogger: {hours, temp_count, event_count, flash_kb} |
| /api/wifi/ap | GET/POST | WiFi AP mode settings (SSID, password, channel) |
| /api/time | GET/POST | Поточний час / NTP налаштування (timezone, NTP server) |
| /api/factory-reset | POST | Factory reset (очистка NVS + restart) |
| /api/restart | POST | ESP restart |
| /ws | WS | Real-time state broadcast |
| /* | GET | Static files (LittleFS) |

### WebSocket
- **Delta broadcasts:** тільки змінені ключі (~200B замість 3.5KB full state)
- Broadcast interval: 1500ms (BROADCAST_INTERVAL_MS)
- Heartbeat PING кожні 20s (HEARTBEAT_INTERVAL_MS = 20000)
- Manual control frames (PING/PONG/CLOSE)
- Max 3 concurrent clients (MAX_WS_CLIENTS = 3)
- Новий клієнт отримує full state при підключенні (send_full_state_to)

### WiFi
- STA mode: підключення до існуючого роутера (credentials з NVS)
- AP fallback: ModESP-XXXX (якщо STA не вдалося)
- IP: залежить від режиму (STA = DHCP, AP = 192.168.4.1)

### Heap Optimization
- **WS heap guard:** 16KB (full state) / 8KB (delta/ping) мінімум перед malloc
- **NVS batch API:** один handle для flush_to_nvs() замість 30+ nvs_open/close
- **Float rounding:** 0.01°C precision зменшує SharedState version bumps
- **Delta broadcasts:** ~200B payload замість 3.5KB → менше heap pressure
- **track_change=false:** таймери/діагностика не тригерять WS broadcasts
- **system.heap_largest:** моніторинг найбільшого вільного блоку (фрагментація)

---

## Правила документування (ОБОВ'ЯЗКОВО)

### Git — коміти та push після КОЖНОЇ зміни

**ОБОВ'ЯЗКОВО:** після завершення кожної задачі або логічного блоку змін Claude Code ПОВИНЕН:
1. `git add` — додати змінені файли
2. `git commit` — conventional commits, опис українською, деталізація в body
3. `git push origin main` — ЗАВЖДИ пушити на remote

Не відкладай push на "потім". Кожен коміт = push.

Формат commit message:
```
feat(module): короткий опис

- Деталь 1
- Деталь 2
- Closes BUG-XXX (якщо є)
```

Приклади:
- `feat(defrost): runtime validation — fallback to natural without hg_valve`
- `fix(wifi): country code UA (channels 1-13), scan time 500ms`
- `refactor(equipment): publish has_defrost_relay to SharedState`
- `docs: CLAUDE_TASK_13a — settings validation 3 levels`

Якщо зміни великі — кілька атомарних комітів замість одного великого.

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
| `docs/07_equipment.md` | Equipment Manager + Protection | При зміні EM/Protection логіки |
| `docs/08_webui.md` | Svelte WebUI архітектура | При зміні WebUI структури |
| `docs/09_datalogger.md` | DataLogger + ChartWidget | При зміні DataLogger |
| `docs/10_manifest_standard.md` | Стандарт маніфестів | При зміні формату manifest |
| `docs/CHANGELOG.md` | Повний changelog проекту | Після кожної сесії |
| `tests/host/` | Host C++ unit tests (doctest) | При зміні бізнес-логіки |
| `next_prompt.md` | Промпт для наступної сесії | В кінці поточної сесії |

## Changelog
- 2026-03-07 — WebUI Premium Redesign R1: premium dark theme bento-card dashboard, card icons (shield/flame/thermometer/database), responsive accordions (GroupAccordion, desktop open / mobile collapsed), System & Network pages restructure, widget grouping, duplicate card removal, uptime HH:MM:SS. Bundle: 63KB JS + 13KB CSS gz. i18n: ~120 keys per language. 32 Svelte компоненти.
- 2026-03-02 — Phase 17 Phase 1: Compressor Safety in Protection Module. 5 new alarm monitors (short cycle,
  rapid cycle, continuous run, pulldown failure, rate-of-change). CompressorTracker (ring buffer 30 starts,
  sliding 1h window). RateTracker (EWMA lambda=0.3). Motor hours tracking. 2 features (compressor_protection,
  rate_protection). 18 new state keys, 122 total. 61 STATE_META, 48 MQTT pub, 60 MQTT sub. 10 new test cases.
  Alarm priority: 11 levels. Defrost interaction: rate blocked during heating+post-defrost.
- 2026-03-01 — Рефакторинг документації: приведення до реального стану коду. Виправлено stale refs,
  параметри хв/с, старі абревіатури, мертві посилання, додано відсутні HTTP endpoints.
- 2026-03-01 — Phase 12a DONE: KC868-A6 board support (I2C PCF8574 expander, pcf8574_relay + pcf8574_input drivers).
  defrost_relay merger (heater+hg_valve→defrost_relay). NVS batch API. WS heap guard 40KB.
  Float rounding 0.01°C. WS broadcast 3000ms. Heap diagnostics (system.heap_largest).
  6 drivers, 53 STATE_META, 52 MQTT sub, 90 host C++ tests.
- 2026-02-24 — Phase 14b DONE: 6-channel dynamic DataLogger + ChartWidget. TempRecord 12→16 bytes (ch[6] array),
  ChannelDef compile-time table (air/evap/cond/setpoint/humidity/reserved), sync/sample/serialize loops.
  Manifest: +log_setpoint, +log_humidity, "Канали логування" card. ChartWidget: fully dynamic channels
  from API, PALETTE colors, toggle checkboxes, setpoint dual-mode. JSON v3 with dynamic channels.
  i18n: +chart.ch_setpoint, +chart.ch_humidity. 97 state keys, 48 STATE_META, 46 MQTT sub, 264 тестів.
- 2026-02-24 — Phase 7b-c DONE: WebUI Polish. Light/Dark theme toggle (stores/theme.js, CSS custom properties,
  localStorage persist, prefers-color-scheme). i18n UA/EN (stores/i18n.js, ~75 keys × 2 languages, derived $t store).
  Animations: page fly/fade transitions, staggered card entrance, card slide collapse, value flash on change.
  19 files updated. Bundle: 37.5→43.7KB gz (within 50KB limit).
- 2026-02-24 — Phase 14a: Multi-channel DataLogger (air+evap+cond), TempRecord 8→12 bytes,
  TEMP_NO_DATA sentinel, JSON v2 з channels header, auto-migration old format.
  ChartWidget: multi-line chart (3 polylines), channel toggles, event text list (50 events),
  CSV export (client-side). Equipment: +has_cond_temp. Generator: +cond_temp FEATURE_TO_STATE.
  Fix: Cache-Control no-store (was max-age=86400 causing stale bundle).
  95 state keys, 46 STATE_META, 37 MQTT pub, 44 MQTT sub, 207 тестів.
- 2026-02-24 — Phase 14 DONE: DataLogger module (append+rotate LittleFS, streaming chunked JSON,
  10 event types, 6 state keys) + ChartWidget (SVG polyline, min/max downsample, comp/defrost zones,
  tooltip, 24h/48h toggle). GET /api/log, GET /api/log/summary. downsample.js utility.
  Tech debt: TIMER_SATISFIED, Cache-Control, AUDIT-012 separate alarm delays, AUDIT-036 CLOSED.
  92 state keys, 44 STATE_META, 37 MQTT pub, 42 MQTT sub, 207 тестів. 5 modules, 9 pages.
- 2026-02-23 — Phase 13a DONE: Runtime UI visibility (visible_when + requires_state). Manifests: constraints
  з disabled_hint, visible_when на defrost/thermostat/protection cards/widgets. Generator: resolve_constraints()
  зберігає ВСІ options + requires_state (FEATURE_TO_STATE mapping), visible_when passthrough, V19 validation.
  Svelte: isVisible() utility, SelectWidget per-option disabled, DynamicPage visible_when. Equipment: +3 has_* keys
  (has_cond_fan, has_door_contact, has_evap_temp). 84 state keys, 178 тестів. Runtime: Bindings→Save→Restart→enabled.
- 2026-02-23 — Phase 11b COMPLETE: SEARCH_ROM (Maxim AN187 binary search), GET /api/onewire/scan endpoint
  (scan bus → devices with temperature + assigned status), WebUI OneWire Discovery in BindingsEditor
  (scan button, device list, role assignment). HttpService: set_hal() injection for scan.
- 2026-02-22 — Phase 11b DONE: Multi DS18B20 (MATCH_ROM + CRC8 validation), NTC/ADC driver (B-parameter),
  DigitalInput C++ driver (50ms debounce). HAL: Binding.address, GpioInputConfig, AdcChannelConfig.
  config_service: parse address/gpio_inputs/adc_channels. DriverManager: digital_input + ntc pools.
  Equipment: condenser_temp (NTC/DS18B20) + door_contact (DigitalInput). 5 drivers.
  81 state keys (was 80), 39 STATE_META, 34 MQTT pub, 38 MQTT sub. 206 tests green.
- 2026-02-21 — Phase 11a DONE: Night Setback (4 modes, SNTP schedule, DI, manual),
  Post-defrost alarm suppression (0-120 min timer), Display during defrost (real/frozen/-d-).
  Equipment: night_input role + digital input binding. Thermostat: effective_setpoint, display_temp,
  is_night_active(). Protection: post_defrost_delay, suppress_high flag. Dashboard: display_temp + NIGHT badge.
  80 state keys (was 70), 39 STATE_META, 33 MQTT pub, 38 MQTT sub, 13 menu items. 206 tests green.
- 2026-02-20 — Phase 10.5 DONE: Features System + Select Widgets. Manifests: features/constraints/options
  in thermostat/defrost/protection. Generator: FeatureResolver, select widgets, disabled+reason,
  FeaturesConfigGenerator → features_config.h (5th artifact). C++: has_feature() in BaseModule,
  guards in thermostat/defrost/protection. digital_input driver manifest. board.json: 4 relays, 1 DI, 2 ADC.
  Validation V14-V18. 209 pytest tests green (43 new in test_features.py + 4 binding fixtures).
- 2026-02-20 — BUG-012: NVS positional keys → hash-based (djb2). Auto-migration p0..p32 → sXXXXXXX.
  BUG-023: POST /api/settings uses meta->type instead of decimal point heuristic. Float persist fixed.
  AUDIT-014..017: manifest range fixes. AUDIT-038..040: security (CORS, traversal, old files removed).
- 2026-02-20 — AUDIT Phase 10: 10 critical fixes (relay min_switch_ms, JSON escaping, WebUI icons/tiles/alarm).

> Повний changelog (2026-02-16 — 2026-03-01): [docs/CHANGELOG.md](docs/CHANGELOG.md)
