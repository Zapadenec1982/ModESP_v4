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
- `data/www/bundle.js.gz` — Svelte app gzipped (~14KB)
- `data/www/bundle.css.gz` — Styles gzipped (~3KB)
- Build: `cd webui && npm run build` → Deploy: `npm run deploy` → data/www/

### Equipment Layer (Phase 9.1)
- **EquipmentModule** (priority=CRITICAL) — єдиний модуль з доступом до HAL drivers
- Читає сенсори → публікує `equipment.air_temp`, `equipment.sensor1_ok`, etc.
- Читає requests від бізнес-модулів: `thermostat.req.compressor`, `defrost.req.*`, `protection.lockout`
- Арбітраж: Protection LOCKOUT > Defrost active > Thermostat
- Інтерлоки: тен↔компресор ніколи одночасно, тен↔клапан ГГ ніколи одночасно
- **Compressor anti-short-cycle (output-level):** COMP_MIN_OFF_MS=180s, COMP_MIN_ON_MS=120s
  Захищає компресор незалежно від джерела запиту (thermostat/defrost)
- Публікує **фактичний** стан реле через `get_state()` (не бажаний `out_`)
- Relay `min_switch_ms` = 0 для всіх реле крім compressor (180s) — heater/fan/valve перемикаються миттєво
- **Night input:** опціональний дискретний вхід для нічного режиму → `equipment.night_input`
- Бізнес-модулі (Thermostat, Defrost, Protection) працюють ТІЛЬКИ через SharedState
- `data/bindings.json` — всі drivers прив'язані до module "equipment"

### Thermostat v2 (Phase 9.2 + 11a)

- **Асиметричний диференціал:** ON при T >= effective_SP + differential, OFF при T <= effective_SP
- **State machine:** STARTUP → IDLE ↔ COOLING, SAFETY_RUN (при відмові датчика)
- **Захист компресора:** min_on_time (cOt), min_off_time (cFt), startup_delay
- **Safety Run:** циклічна робота (safety_run_on / safety_run_off) при sensor1_ok=false
- **Вентилятор випарника:** 3 режими (постійно / з компресором / за T_evap з гістерезисом)
- **Вентилятор конденсатора:** синхронно з компресором + затримка OFF (cond_fan_delay)
- **Night Setback:** 4 режими (0=off, 1=schedule SNTP, 2=DI, 3=manual), effective_sp = setpoint + night_setback
- **Display during defrost:** 3 режими (0=real T, 1=frozen T, 2="-d-" symbol) → `thermostat.display_temp`
- **16 persist параметрів** (11 base + night_setback, night_mode, night_start, night_end, display_defrost)
- Requests: `thermostat.req.compressor`, `thermostat.req.evap_fan`, `thermostat.req.cond_fan`

### Protection (Phase 9.3 + 11a)

- **5 незалежних моніторів аварій:** High Temp, Low Temp, Sensor1, Sensor2, Door
- **Delayed alarms:** High/Low temp і Door з затримкою (alarm_delay хвилини, door_delay хвилини)
- **Instant alarms:** Sensor1, Sensor2 — без затримки
- **Defrost blocking:** High Temp alarm блокується під час heating-фаз defrost (скидається pending)
- **Post-defrost suppression:** High Temp alarm блокується на post_defrost_delay хвилин після відтайки
- **Auto-clear:** аварія знімається автоматично при поверненні в норму (якщо manual_reset=false)
- **Manual reset:** `protection.reset_alarms` = true → скидає всі аварії (WebUI/API)
- **6 persist параметрів:** high_limit, low_limit, alarm_delay, door_delay, manual_reset, post_defrost_delay
- **protection.lockout = false** завжди (зарезервовано для Phase 10+)
- Порядок update: Equipment(0) → **Protection(1)** → Thermostat(2)

### Defrost (Phase 9.4)

- **7-фазна state machine:** IDLE → [STABILIZE → VALVE_OPEN →] ACTIVE → [EQUALIZE →] DRIP → FAD → IDLE
- **3 типи відтайки (dFT):** 0=природна (зупинка), 1=тен, 2=гарячий газ (7 фаз)
- **4 ініціації:** таймер (dit з dct=1 реальний/dct=2 компресор), demand (T_evap<dSS), комбінований, ручний
- **Завершення:** по T_evap >= dSt (основна), по таймеру безпеки dEt, consecutive timeout counter
- **Оптимізація:** skip defrost якщо T_evap > dSt (випарник чистий)
- **FAD:** Fan After Defrost — компресор+конд.вент ON, вент.вип OFF, завершення по FAT або таймеру FAd
- **13 persist параметрів** + 2 runtime persist (interval_timer, defrost_count)
- **NVS persistence:** interval_timer і defrost_count зберігаються через PersistService
- Requests: `defrost.req.compressor`, `defrost.req.heater`, `defrost.req.evap_fan`, `defrost.req.cond_fan`, `defrost.req.hg_valve`
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
- **Drivers:** ds18b20 (sensor, MATCH_ROM multi-sensor), relay (actuator), digital_input (sensor, GPIO input), ntc (sensor, ADC thermistor)
- **Validation:** V14 (controls_settings exist), V15 (requires_roles exist), V16 (requires_feature exist), V17 (options int), V18 (options→select)
- **209 pytest тестів** (43 нових у test_features.py + 4 binding fixtures)

### Файли які МОЖНА редагувати
- `modules/*/manifest.json` — опис модулів (UI, state, mqtt, display, features, constraints)
- `drivers/*/manifest.json` — опис драйверів (category, hardware_type, settings)
- `data/board.json` — PCB pin assignment (gpio_outputs, onewire_buses, gpio_inputs, adc_channels)
- `data/bindings.json` — Runtime: role → driver → GPIO mapping
- `tools/generate_ui.py` — генератор (~1200 рядків, генерує 5 артефактів)
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
- `constraints` секція фільтрує options: `enum_filter` → requires_feature → active features
- Приклад: defrost.type options фільтруються — без heater → тільки "За часом"

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
│   └── digital_input/
│   │   ├── manifest.json      # ⭐ Driver manifest (sensor, gpio_input, 1 setting)
│   │   └── src/...            # GPIO input with 50ms debounce
│   ├── ds18b20/
│   │   ├── manifest.json      # ⭐ Driver manifest (sensor, onewire_bus, 3 settings)
│   │   └── src/...            # Dallas DS18B20 (SKIP_ROM + MATCH_ROM multi-sensor)
│   ├── ntc/
│   │   ├── manifest.json      # ⭐ Driver manifest (sensor, adc_channel, 5 settings)
│   │   └── src/...            # NTC thermistor via ADC (B-parameter equation)
│   └── relay/
│       ├── manifest.json      # ⭐ Driver manifest (actuator, gpio_output, 2 settings)
│       └── src/...            # GPIO relay with min on/off time protection
├── modules/
│   ├── equipment/
│   │   ├── manifest.json      # ⭐ Equipment Manager (HAL owner, arbitration)
│   │   └── src/equipment_module.cpp
│   ├── protection/
│   │   ├── manifest.json      # ⭐ Alarm monitoring (5 monitors, dAd delay)
│   │   └── src/protection_module.cpp
│   ├── thermostat/
│   │   ├── manifest.json      # ⭐ Single Source of Truth для UI/state/mqtt
│   │   └── src/thermostat_module.cpp
│   └── defrost/
│       ├── manifest.json      # ⭐ 7-phase defrost cycle (3 types, 13 params)
│       └── src/defrost_module.cpp
├── tools/
│   ├── generate_ui.py         # Manifest → UI + C++ headers generator (~1200 lines)
│   └── tests/                 # 209 pytest tests (test_features, test_modules, test_validator)
├── data/
│   ├── board.json             # PCB pin assignment (gpio_outputs, onewire_buses, gpio_inputs, adc_channels)
│   ├── bindings.json          # Runtime: role → driver → GPIO mapping (manifest_version: 1)
│   ├── ui.json                # 🔄 Generated
│   └── www/                   # Deployed WebUI (index.html, bundle.js.gz, bundle.css.gz)
├── webui/                     # Svelte 4 WebUI source
│   ├── src/                   # App.svelte, stores/, lib/, components/, pages/
│   ├── scripts/deploy.js      # Gzip + copy to data/www/
│   ├── package.json           # npm run build / npm run deploy
│   └── rollup.config.js       # Rollup bundler config
├── generated/                 # 🔄 All generated C++ headers (5 files)
├── docs/                      # Architecture docs (01-09)
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
- 2026-02-20 — AUDIT Phase 10: 10 critical fixes. C++: relay min_switch_ms role-based (compressor only),
  EM publishes actual relay state via get_state(), EM-level compressor anti-short-cycle timer
  (COMP_MIN_OFF_MS=180s, COMP_MIN_ON_MS=120s), JSON string escaping in http_service + ws_service.
  WebUI: ButtonWidget state key fallback (manual defrost works), icons (flame, shield-alert,
  alert-triangle, thermometer), Dashboard uses equipment.compressor + defrost/alarm tiles,
  StatusText defrost phase colors, alarm banner in Layout, apiPost error handling.
- 2026-02-18 — SharedState capacity 64→96 (MODESP_MAX_STATE_ENTRIES). 69 manifest keys + ~15 system keys overflowed 64.
- 2026-02-18 — Phase 9.4 DONE: Defrost module (modules/defrost/). 7-phase state machine,
  3 types (natural/heater/hot gas), 4 initiations (timer/demand/combo/manual).
  13 persist params + 2 runtime persist (interval_timer, defrost_count). 27 state keys, 10 MQTT publish.
  Generator fix: read-only persist keys now included in state_meta.h (writable=false, persist=true).
  4 modules, 69 state keys, 9 pages. 79 тестів зелені.
- 2026-02-18 — Phase 9.3 DONE: Protection module (modules/protection/). 5 alarm monitors
  (HAL, LAL, ERR1, ERR2, Door). Delayed alarms (dAd), defrost blocking, auto-clear + manual reset.
  5 persist параметрів, 14 state keys, 8 MQTT publish. 79 тестів зелені. 3 modules, 42 state keys.
- 2026-02-18 — Phase 9.2 DONE: Thermostat v2 — повна логіка spec_v3. Асиметричний диференціал,
  state machine (STARTUP→IDLE→COOLING→SAFETY_RUN), вент. випарника (3 режими FAn), вент. конденсатора
  (затримка COd), Safety Run, 11 persist параметрів, 18 state keys. 79 тестів зелені.
- 2026-02-18 — Phase 9.1 DONE: Equipment Manager (modules/equipment/). Єдиний власник HAL drivers.
  Арбітраж: Protection > Defrost > Thermostat. Інтерлоки: тен↔компресор, тен↔клапан ГГ.
  Thermostat рефакторинг: req.compressor замість direct relay, читає equipment.air_temp.
  generate_ui.py: cross-module widget key resolution (inputs → global state map). 79 тестів зелені.
- 2026-02-17 — Phase 7a DONE: Svelte WebUI (webui/). Svelte 4 + Rollup. 14 widget components,
  Dashboard (tile-based, temp color zones, compressor pulse), Layout (sidebar + bottom tabs),
  DynamicPage (renders any ui.json page). Bundle: 17KB gzipped. Deploy: npm run deploy → data/www/.
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
