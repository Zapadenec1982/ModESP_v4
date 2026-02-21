# ModESP v4 — План дій

> Цей документ — твоя точка входу в проект.
> Відкривай його на початку кожної робочої сесії.
> Оновлюй після кожної завершеної задачі.

---

## Де ми зараз

**Milestone M1: "Лабораторний прототип" — ДОСЯГНУТО.**
**Phase 5c: "Стабілізація" — ЗАВЕРШЕНА.**
**Phase 6: "MQTT + OTA" — ЗАВЕРШЕНА (MQTT повністю реалізований, OTA працює, WebUI сторінки додані).**

**Phase 6.5: "Auto-persist settings" — ЗАВЕРШЕНА.**

**Phase 9: "Equipment Layer + Промислові модулі" — ЗАВЕРШЕНА.**
**Phase 10 (Audit + Bugfixes): Стабілізація — В ПРОЦЕСІ.**

**► Phase 10.5 — Features System + Select Widgets — ЗАВЕРШЕНА.**

### Що РЕАЛЬНО працює (підтверджено boot log 2026-02-17):
- ✅ Ядро: 11 модулів, трифазний boot за 1.2с
- ✅ HAL + Drivers: DS18B20 (GPIO4), Relay (GPIO2), bindings з board.json
- ✅ Thermostat: setpoint=-19.5°C (з NVS!), hysteresis=1.5°C, компресор перемикається
- ✅ WiFi STA: підключення до роутера ASUS (192.168.1.32), AP як fallback
- ✅ HTTP: 12 endpoints + OTA upload + static file handler (/* → /data/www/)
- ✅ WebSocket: /ws real-time broadcast
- ✅ **MQTT TLS: підключення до HiveMQ Cloud (mqtts://:8883), cert validated, subscribe cmd topics**
- ✅ **SNTP: працює (TZ=EET-2EEST, український часовий пояс)**
- ✅ **PersistService: auto-restore з NVS при boot, auto-save з debounce 5sec**
- ✅ **POST /api/settings: валідація через state_meta (accepted/rejected)**
- ✅ OTA partition: factory(1.5MB) + ota_0(1.5MB) розведені, rollback
- ✅ LittleFS: board.json, bindings.json, ui.json, www/ — все монтується
- ✅ Free heap після init: 176KB (з 240KB) — комфортно
- ✅ 209 pytest тестів для генератора (всі зелені, включаючи features, select widgets, constraints)
- ✅ HTMLGenerator видалений — WebUI статичний (data/www/), generate_ui.py: ~900 рядків, 4 артефакти
- ✅ Driver manifests: ds18b20 (sensor) + relay (actuator) з валідацією
- ✅ board.json: оновлений стандарт (relays→gpio_outputs, manifest_version)
- ✅ Bindings page: сторінка "Обладнання" в WebUI (access_level: service)
- ✅ Cross-валідація module↔driver в генераторі
- ✅ MQTT: повна реалізація (528 рядків) — NVS config, esp_mqtt_client, publish/subscribe, LWT, HTTP API
- ✅ MQTT WebUI: сторінка конфігурації (статус + форма broker/port/user/pass/prefix/enabled)
- ✅ OTA: upload endpoint (POST /api/ota), dual-partition, rollback, UI з progress bar
- ✅ WiFi PS bug виправлений — DS18B20 більше не маніпулює esp_wifi_set_ps()

- ✅ **Auto-persist settings (Phase 6.5): PersistService + SharedState callback + state_meta validation**

### Що НЕ працює / не реалізовано:
- ❌ MQTT auto-discovery (Home Assistant)
- ❌ LCD/OLED display
- ❌ mDNS (hostname.local)
- ❌ Modbus RTU/TCP
- ❌ Temperature graphs (Phase 7b-c)
- ❌ HTTP authentication

---

## ✅ Phase 6 — MQTT + OTA (ЗАВЕРШЕНО)

### 2.0 Дрібні борги
```
  1. [x] Створити drivers/ds18b20/manifest.json (DONE — 2026-02-17)
  2. [x] Створити drivers/relay/manifest.json (DONE — 2026-02-17)
  3. [x] Оновити board.json на новий стандарт (gpio_outputs) (DONE — 2026-02-17)
  4. [x] DriverManifestValidator + Loader + cross-валідація (DONE — 2026-02-17)
  5. [x] Bindings page в ui.json (DONE — 2026-02-17)
  6. [x] C++ HAL: relays→gpio_outputs (DONE — 2026-02-17)
  7. [x] Оновити docs/06_roadmap.md (DONE — 2026-02-17)
  8. [x] Оновити next_prompt.md (DONE — 2026-02-17)
```

### 2.1 MQTT — ПОВНІСТЮ РЕАЛІЗОВАНИЙ (528 рядків C++)
```
  [x] Конфігурація broker через /api/mqtt POST → NVS
  [x] Підключення до брокера (esp_mqtt_client) з LWT
  [x] Publish state keys з mqtt_topics.h при зміні version
  [x] Subscribe на readwrite keys → SharedState set() з валідацією min/max
  [x] Auto-reconnect (stop_client → start_client)
  [x] Статус в SharedState (mqtt.connected, mqtt.status, mqtt.broker)
  [x] MQTT WebUI page: статус + форма налаштувань
```

### 2.2 MQTT auto-discovery — НЕ РЕАЛІЗОВАНО (перенесено)
```
  1. [ ] HA discovery topics (homeassistant/sensor/modesp_xxx/config)
  2. [ ] Device info (manufacturer: ModESP, model, sw_version)
  Перенесено на Phase 7 або пізніше.
```

### 2.3 OTA — ПОВНІСТЮ РЕАЛІЗОВАНИЙ
```
  [x] POST /api/ota — прийом .bin файлу (chunked, 1KB)
  [x] esp_ota_begin/write/end з CRC валідацією
  [x] Auto-rollback при boot failure (esp_ota_mark_app_valid)
  [x] UI: firmware_upload widget з progress bar
  [x] Версія прошивки в SharedState (_ota.version, _ota.partition)
```

**Milestone M2: "Connected Controller" — ДОСЯГНУТО.**

---

## ✅ Phase 6.5 — Auto-persist settings (ЗАВЕРШЕНО)

### Архітектурні рішення (прийняті 2026-02-17, оновлені 2026-02-18)

| Тема | Рішення | Обґрунтування |
|------|---------|---------------|
| **Equipment Layer** | Єдиний Equipment Manager володіє всіма drivers. Модулі працюють через SharedState requests | Вирішує: relay ownership між модулями, інтерлоки в одному місці, rL1-rL4 конфігурація, protection lockout |
| **Module arbitration** | EM арбітраж: Protection > Defrost > Thermostat. Інтерлоки hardcoded в EM | Модулі публікують req.*, EM вирішує. Гарантовано безпечна поведінка |
| **Persist settings** | Auto-persist на рівні фреймворку (SharedState → NVS) | Кожен модуль потребує persist, дублювання NVS логіки неприпустиме |
| **Timers** | Кожен модуль сам (millis + state machine в on_update) | Промислові таймери = domain state machine, загальний scheduler не дає користі |
| **Data logging** | Окремий DataLogger module з inputs | Manifest-driven: що логувати описано в inputs. Не вбудований у фреймворк |

### КРОК 3: Auto-persist settings — DONE
```
  1. [x] SharedState.set() → persist callback ПОЗА mutex → PersistService dirty flag
  2. [x] PersistService: boot restore з NVS → SharedState (з defaults з state_meta)
  3. [x] PersistService: debounce 5s → flush dirty keys в NVS (flash wear protection)
  4. [x] state_meta.h: persist + default_val поля в StateMeta struct
  5. [x] POST /api/settings: валідація через state_meta (writable, min/max clamp)
  6. [x] Thermostat: прибрано hardcoded config.setpoint, читає з SharedState (populated by PersistService)
  7. [x] 79 pytest тестів зелені (6 нових для persist)
```

---

## Наступна задача: КРОК 4 — Svelte WebUI (Phase 7)

### КРОК 4: Svelte WebUI (Phase 7a — DONE)

```
  1. [x] Svelte 4 проект (webui/), Rollup build pipeline (DONE — 2026-02-17)
  2. [x] WebSocket store (reactive state, auto-reconnect) (DONE — 2026-02-17)
  3. [x] Tile-based Dashboard (CSS Grid, color zones, компресор пульсація) (DONE — 2026-02-17)
  4. [x] 14 widget components (value, slider, number_input, indicator, status_text,
         toggle, text_input, password_input, button, firmware_upload, wifi_save,
         mqtt_save, time_save, datetime_input) (DONE — 2026-02-17)
  5. [x] Layout: sidebar (desktop) + bottom tabs (mobile) + Lucide SVG icons (DONE — 2026-02-17)
  6. [x] DynamicPage: renders any page from ui.json (DONE — 2026-02-17)
  7. [x] Build: 17KB gzipped (target <60KB) (DONE — 2026-02-17)
  8. [x] Deploy script: npm run deploy → gzip → data/www/ (DONE — 2026-02-17)
  9. [x] Спростити generate_ui.py → HTMLGenerator видалений, WebUI статичний
```

---

### КРОК 5: Equipment Layer + Промислові модулі (Phase 9)

**Архітектурне рішення (2026-02-18): Equipment Layer**

Замість прямого доступу модулів до HAL/drivers — єдиний Equipment Manager (EM)
володіє всіма сенсорами і актуаторами. Бізнес-модулі працюють ТІЛЬКИ через SharedState:
публікують requests і читають стани.

```
  Thermostat          Defrost           Protection
      │                  │                  │
      │ req.compressor   │ req.heater       │ lockout
      ▼                  ▼                  ▼
  ┌──────────────────────────────────────────────┐
  │           Equipment Manager (EM)              │
  │                                               │
  │  Арбітраж: Protection > Defrost > Thermostat  │
  │  Інтерлоки: тен↔компресор, клапан→компресор  │
  │  Relay assignment: rL1-rL4 конфігурація       │
  └──────────────────────────────────────────────┘
      │           │           │           │
      ▼           ▼           ▼           ▼
    Relay1      Relay2      Relay3      Relay4
    Sensor1     Sensor2     DI(door)    ...
```

**Потік даних:**
1. EM.on_update(): застосовує relay requests з попереднього циклу
2. EM.on_update(): читає сенсори → публікує equipment.air_temp, etc.
3. Protection.on_update(): перевіряє аварії → публікує protection.lockout
4. Thermostat.on_update(): читає temp → публікує thermostat.req.compressor
5. Defrost.on_update(): state machine → публікує defrost.req.heater
6. Наступний цикл: EM застосовує нові requests

**Пріоритет арбітражу:** Protection LOCKOUT > Defrost active > Thermostat

**Інтерлоки (hardcoded в EM):**
- Тен і компресор НІКОЛИ одночасно
- Клапан ГГ відкривається ДО компресора
- Protection lockout = все вимкнено

**Порядок реалізації:**
```
  5.1 [x] Equipment Manager — новий модуль, забирає HAL (DONE — 2026-02-18)
          - Читає сенсори → SharedState
          - Читає req.* → relay з арбітражем
          - Рефакторинг Thermostat: req замість direct relay
          - bindings.json: module="equipment"
          
  5.2 [x] Thermostat v2 — повна логіка зі spec_v3 (DONE — 2026-02-18)
          - State machine: STARTUP → IDLE → COOLING → SAFETY_RUN
          - Асиметричний диференціал (ON: T>=SP+rd, OFF: T<=SP)
          - Вент. випарника (3 режими FAn), вент. конденсатора (затримка COd)
          - Safety Run (ERR1: циклічний пуск при відмові датчика)
          - 11 persist параметрів, 18 state keys, 79 тестів зелені
          
  5.3 [x] Protection — захист і аварії (DONE — 2026-02-18)
          - 5 alarm monitors: HAL, LAL, ERR1, ERR2, Door
          - Delayed alarms (dAd), defrost blocking for high temp
          - Auto-clear + manual reset (protection.reset_alarms)
          - 5 persist params, 14 state keys, 8 MQTT publish
          - protection.lockout reserved (always false)
          
  5.4 [x] Defrost — цикл розморозки (DONE — 2026-02-18)
          - 7-phase state machine: IDLE→STABILIZE→VALVE_OPEN→ACTIVE→EQUALIZE→DRIP→FAD
          - 3 типи: зупинка (dFT=0) / тен (dFT=1) / гарячий газ (dFT=2, 7 фаз)
          - 4 ініціації: таймер / demand / комбінований / ручний
          - 13 persist params + 2 runtime persist (interval_timer, defrost_count)
          - 27 state keys, 10 MQTT publish
          - Generator fix: read-only persist keys в state_meta.h
```

**Milestone M3: "Production Ready" — після Phase 7 + 9.**

---

### КРОК 5.5: Bugfixes (Phase 9 post-stabilization)

**Документ:** `docs/BUGFIXES_VERIFIED.md`

Виявлено 27 багів (24 оригінальні + 3 нові під час верифікації).

```
  Сесія #1 (2026-02-20):
  [x] NEW-002, NEW-003 — MAX_PERSIST_KEYS 16→48
  [x] BUG-001, BUG-011 — Key length 24→32, буфери 2048→3072
  [x] BUG-002 — Cache defrost_type at cycle start
  [x] BUG-004 — COd delay during defrost
  [x] NEW-001 — Equipment SAFE_MODE msg_id

  Сесія #2 (2026-02-20):
  [x] NEW-004 — JSON parsing bounds check (3 loops)
  [x] BUG-008 — Demand defrost min interval (25% rule)
  [x] BUG-010 — defrost.heater_alarm при 3 timeouts
  [x] BUG-024 — init_all() returns false on CRITICAL failure
  [x] BUG-009 — Compressor timer по equipment.compressor (actual)
  [x] BUG-007 — HAL alarm → тільки heating phases defrost
  [x] BUG-017 — version++ тільки при реальній зміні

  Залишено OPEN (SEV-2, нижчий пріоритет):
  [ ] BUG-005 — Thermostat state inconsistency при defrost
  [ ] BUG-006 — enter_phase() без re-entry guard
  [ ] BUG-021 — WS client_fds_ race condition
  [x] BUG-023 — POST type heuristic замість meta->type. DONE 2026-02-20

  Технічний борг (SEV-3/4, не блокує):
  [x] BUG-012 — NVS positional keys coupling → hash-based keys. DONE 2026-02-20
  [ ] BUG-013 — read helpers duplication
  [ ] BUG-014 — No immediate NVS flush
  [ ] BUG-016 — String-based phase compare
  [ ] BUG-018 — set() return value ignored
```

**Підсумок: 14/27 FIXED, 11 OPEN, 2 BY DESIGN.**

---

### КРОК 6: Аудит бізнес-логіки + WebUI (Phase 10 — Stabilization)

**Аудит виконаний 2026-02-20** командою з 3 агентів:
- Експерт з холодильного обладнання (Danfoss/Dixell досвід)
- UX/UI дизайнер промислових систем
- Frontend розробник (Svelte + embedded web)

#### Пріоритет 1 — КРИТИЧНІ (блокують деплой)

```
  C++ бізнес-логіка:
  [x] AUDIT-001 — min_switch_ms=180000 на ВСІХ реле (driver_manager.cpp:158). DONE 2026-02-20
      FIX: role-based min_switch_ms (0 для не-компресор, 180000 тільки для compressor)
  [x] AUDIT-002 — relay.set() return value ігнорується в apply_outputs(). DONE 2026-02-20
      FIX: перевіряти return + публікувати actual стан через get_state()
  [x] AUDIT-003 — Немає захисту компресора на рівні EM. DONE 2026-02-20
      FIX: EM-level compressor anti-short-cycle (COMP_MIN_OFF_MS=180s, COMP_MIN_ON_MS=120s)

  JSON/API:
  [x] AUDIT-004 — JSON string values не ескейпляться. DONE 2026-02-20
      FIX: json_escape_str() в http_service.cpp + ws_json_escape() в ws_service.cpp

  WebUI:
  [x] AUDIT-005 — Кнопка "Запустити розморозку" не працює. DONE 2026-02-20
      FIX: ButtonWidget fallback: POST /api/settings з {key: true} коли немає api_endpoint
  [x] AUDIT-006 — Іконки flame, shield-alert відсутні в icons.js. DONE 2026-02-20
      FIX: додано flame, shield-alert, alert-triangle, thermometer
  [x] AUDIT-007 — Dashboard показує thermostat.compressor (завжди Off). DONE 2026-02-20
      FIX: equipment.compressor + defrost/alarm тайли на Dashboard
  [x] AUDIT-008 — StatusText без кольорів для фаз defrost. DONE 2026-02-20
      FIX: додано stabilize/valve_open/active/equalize/drip/fad кольори
  [x] AUDIT-009 — Немає alarm banner на всіх сторінках. DONE 2026-02-20
      FIX: persistent alarm banner в Layout.svelte (клік → Protection)
  [x] AUDIT-010 — apiPost не перевіряє response status. DONE 2026-02-20
      FIX: if (!r.ok) throw Error з текстом відповіді
```

#### Пріоритет 2 — ВАЖЛИВІ (перед production)

```
  [ ] AUDIT-011 — Post-defrost alarm suppression timer (30-60 хв)
  [ ] AUDIT-012 — Separate alarm_delay для HAL і LAL
  [ ] AUDIT-013 — Door sensor hardcoded false, потрібна підтримка DI
  [x] AUDIT-014 — defrost.end_temp min: 0→-5, defrost.fad_temp max: 0→+10. DONE 2026-02-20
  [x] AUDIT-015 — thermostat.min_on_time default: 60→120, min: 0→30. DONE 2026-02-20
  [x] AUDIT-016 — thermostat.differential min: 0.1→0.5. DONE 2026-02-20
  [x] AUDIT-017 — protection.alarm_delay min: 0→5. DONE 2026-02-20
  [x] AUDIT-018 — Enum params (dFT, FAn) показуються як числа, потрібен select widget. DONE 2026-02-20 (Phase 10.5)
  [ ] AUDIT-019 — protection.reset_alarms не підключений в UI (кнопка скидання аварій)
  [ ] AUDIT-020 — WS broadcast: malloc в hot path порушує zero-heap правило
  [ ] AUDIT-021 — Серіалізаційний буфер 3072 може переповнитись при >84 ключах
```

#### Пріоритет 3 — БАЖАНІ (commercial viability)

```
  [ ] AUDIT-030 — HTTP автентифікація (хоча б Basic Auth)
  [ ] AUDIT-031 — High/Low pressure switch (HP/LP) digital inputs
  [ ] AUDIT-032 — Scheduled defrost (за часом доби)
  [ ] AUDIT-033 — Alarm relay output для BMS
  [ ] AUDIT-034 — Password protection для параметрів
  [ ] AUDIT-035 — °C/°F вибір одиниць
  [ ] AUDIT-036 — Compressor lockout після N коротких циклів
  [ ] AUDIT-037 — Cache-Control headers для static files
  [x] AUDIT-038 — Видалити старі app.js/style.css з data/www/ (26KB flash). DONE 2026-02-20
  [x] AUDIT-039 — CORS: видалено * — same-origin only. DONE 2026-02-20
  [x] AUDIT-040 — Directory traversal захист (strstr ".." check). DONE 2026-02-20
```

---

### ► КРОК 6.5: Features System + Select Widgets (Phase 10.5)

**Задача:** Прогресивне розкриття функціональності — UI показує тільки те обладнання,
яке реально підключене. Enum settings показуються як dropdown з людськими назвами.

**Детальний промпт:** `CLAUDE_TASK.md`

**Scope:** ВСЕ ЗАВЕРШЕНО
```
  Маніфести:
  [x] features секція в thermostat/defrost/protection manifests
  [x] constraints секція (enum_filter для defrost.type, initiation, fan_mode)
  [x] options в state definitions (select замість number_input для enum settings)
  [x] labels в equipment.requires (людські назви ролей)
  [x] door_contact + condenser_temp в equipment.requires

  Python генератор (tools/generate_ui.py):
  [x] FeatureResolver клас (bindings → active features)
  [x] Constraints → фільтрація options в select widgets
  [x] disabled + disabled_reason для неактивних features
  [x] FeaturesConfigGenerator → generated/features_config.h
  [x] --bindings CLI аргумент (backward compatible)

  C++ backend (мінімальні зміни):
  [x] base_module.h: has_feature() метод (constexpr lookup)
  [x] thermostat_module.cpp: guards в update_evap_fan(), update_cond_fan()
  [x] defrost_module.cpp: type validation, defrost_by_sensor guard
  [x] protection_module.cpp: door_protection guard

  Тести:
  [x] test_features.py (43 тести: FeatureResolver, constraints, UI disabled, config.h)
  [x] fixtures: bindings_minimal, bindings_with_evap, bindings_with_fans, bindings_full
  [x] Валідація V14-V18

  Інше:
  [x] board.json розширений (4 реле, 1 DI, 2 ADC)
  [x] digital_input driver manifest створений
  [x] Покриває AUDIT-018 (enum → select widget)
  [x] 209 тестів зелені, генератор працює (5 файлів)
```

**НЕ входить в scope:**
- Runtime feature switching (тільки static при генерації)
- Інтерактивний bindings UI (поки статична сторінка)
- Зміни в equipment_module.cpp, http/ws/wifi services
- Discovery endpoint на ESP32

---

### КРОК 7+ (менш терміново)

- **Phase 8:** LCD/OLED display (display_screens.h вже генерується)
- **Phase 10:** Multi-sensor + PID
- **Phase 11:** NTP + mDNS (WiFi STA вже працює!)
- **Phase 12:** Modbus RTU/TCP

---

## Правило одного фокусу

**Працюй тільки над ОДНИМ кроком одночасно.**

```
  [✅ done]       [✅ done]      [✅ done]       [✅ done]        [✅ done]        [✅ done]       [✅ done]       [✅ done]       [✅ done]
Стабілізація  →  MQTT/OTA  → Auto-persist  →  Svelte UI  →  Equipment Mgr  →  Thermostat v2  →  Protection  →  Defrost      →  Features
     ↓              ↓             ↓               ↓                ↓                ↓                ↓              ↓               ↓
  "працює       "можна       "налаштування    "красиво"       "арбітраж       "повна логіка    "аварії       "цикл          "UI показує
   надійно"      розгорнути"  зберігаються"                    relay"           spec_v3"        та захист"   розморозки"    тільки
                                                                                                                           підключене"

  **Phase 9 COMPLETE — всі 4 промислові модулі реалізовані.**
  **Phase 10.5 COMPLETE — Features System + Select Widgets. 209 тестів зелені.**
```

---

## Як працювати з Claude Code

### Початок сесії
1. Claude Code читає `CLAUDE.md` (автоматично)
2. Ти кажеш що хочеш зробити (або даєш промпт-файл)
3. Claude Code працює, ти перевіряєш результат

### Після кожної сесії — Claude Code ОБОВ'ЯЗКОВО:
1. Знімає галочки із завершених задач тут
2. Оновлює CLAUDE.md якщо змінилась архітектура/API
3. Оновлює roadmap якщо завершена фаза
4. Фіксує нові баги/TODO

### Якщо загубився
- **ACTION_PLAN.md** → що робити сьогодні
- **CLAUDE.md** → як працює проект
- **docs/06_roadmap.md** → куди йдемо
- **docs/10_manifest_standard.md** → як писати маніфести

## Changelog
- 2026-02-20 — Phase 10.5 DONE: Features System + Select Widgets. 7 phases completed.
  Manifests: features/constraints/options in thermostat/defrost/protection. Equipment: labels, door_contact, condenser_temp.
  Generator: FeatureResolver, select widgets, disabled+reason, FeaturesConfigGenerator → features_config.h (5th artifact).
  C++: has_feature() in BaseModule, guards in thermostat/defrost/protection. digital_input driver manifest.
  Tests: 43 new (test_features.py) + 4 fixtures. Validation V14-V18. 209 total tests green.
  board.json: 4 relays, 1 DI, 2 ADC. Covers AUDIT-018.
- 2026-02-20 — Phase 10.5 task created: Features System + Select Widgets. CLAUDE_TASK.md написаний (786 рядків).
  Scope: features в маніфестах, FeatureResolver в генераторі, select widgets з options+constraints,
  has_feature() в C++ модулях, features_config.h генерація. Покриває AUDIT-018.
- 2026-02-20 — Quick-wins: 7 fixes (AUDIT-014..017, 038..040). Manifest ranges: defrost.end_temp min -5,
  fad_temp max +10, thermostat.min_on_time default 120/min 30, differential min 0.5, alarm_delay min 5.
  Security: CORS * removed, directory traversal protection, old JS files deleted (26KB flash freed).
- 2026-02-20 — AUDIT session: 10 fixes (AUDIT-001..010). Команда з 3 агентів (refrigeration expert,
  UX designer, frontend dev) знайшла 6 критичних, 16 середніх, 13 низьких проблем.
  C++: relay min_switch_ms per role, equipment actual state publish, EM compressor anti-short-cycle,
  JSON string escaping. WebUI: manual defrost button, missing icons, Dashboard equipment.compressor
  + defrost/alarm tiles, StatusText defrost colors, alarm banner in Layout, apiPost error handling.
- 2026-02-20 — Bugfix session #2: 7 fixes (NEW-004, BUG-007,008,009,010,017,024). Total 14/27 fixed.
  JSON bounds check, demand interval, heater alarm, init_all, compressor timer, HAL phase blocking, version++.
- 2026-02-20 — Bugfix session #1: 5 fixes (BUG-001,002,004,011 + NEW-001,002,003). Verified correct.
- 2026-02-18 — Phase 9.4 DONE: Defrost module. 7-phase state machine, 3 types, 4 initiations.
  13 persist + 2 runtime persist. Generator fix: read-only persist keys в state_meta.h.
  Phase 9 COMPLETE — 4 промислові модулі (equipment, protection, thermostat, defrost).
- 2026-02-18 — Phase 9.3 DONE: Protection module. 5 alarm monitors (HAL, LAL, ERR1, ERR2, Door).
  dAd delayed alarms, defrost blocking, auto-clear + manual reset. 5 persist params, 14 state keys.
  Наступний: 5.4 Defrost.
- 2026-02-18 — Phase 9.2 DONE: Thermostat v2 з повною логікою spec_v3. Асиметричний диференціал,
  state machine, fan control, Safety Run. 11 persist параметрів, 18 state keys. Наступний: 5.3 Protection.
- 2026-02-18 — Phase 9.1 DONE: Equipment Manager створений, Thermostat рефакторинг (req.* замість HAL).
  generate_ui.py: cross-module widget keys. 79 тестів зелені. Наступний крок: 5.2 Thermostat v2.
- 2026-02-18 — Phase 9.3 DONE: Protection — 5 alarm monitors (HAL, LAL, ERR1, ERR2, Door).
  dAd delayed alarms, defrost blocking, auto-clear + manual reset. 318 рядків C++.
  13 модулів, 17 MQTT subscribe topics. Наступний: Defrost.
- 2026-02-18 — Phase 9.2 DONE: Thermostat v2 — повна логіка spec_v3. State machine
  (STARTUP→IDLE→COOLING→SAFETY_RUN), asymmetric differential, evap fan 3 modes,
  cond fan delay, Safety Run, 11 persist params. 476 рядків C++. Наступний: Protection.
- 2026-02-18 — Phase 9.1 DONE: Equipment Manager — єдиний HAL owner. Арбітраж
  (Protection>Defrost>Thermostat), інтерлоки. Thermostat рефакторений на req.*.
  12 модулів, heap 176KB.
- 2026-02-17 — Phase 7a DONE: Svelte WebUI scaffold + Dashboard + WebSocket. 14 widget components,
  sidebar/bottom tabs, Lucide icons, dark theme. Bundle: 17KB gzipped (target <60KB).
  Deploy: webui/ → npm run deploy → data/www/. Vanilla JS prototype залишено як fallback.
- 2026-02-17 — Phase 6.5 DONE: PersistService (restore NVS→SharedState + debounce flush), SharedState persist
  callback, state_meta.h з persist+default_val, POST /api/settings валідація, thermostat без hardcode.
  79 тестів зелені. Наступний крок: Phase 7 (Svelte WebUI).
- 2026-02-17 — Архітектурні рішення: mutual exclusion через inputs, auto-persist framework-level,
  timers per-module, DataLogger як окремий модуль. Phase 6.5 = наступний крок.
  MQTT TLS + SNTP підтверджені boot log. GPIO4 (не 15). 12 endpoints + static handler.
- 2026-02-17 — Inputs validation додана в generate_ui.py (6 правил з docs/10 §3.2a). 73 тести зелені.
- 2026-02-17 — Phase 6 DONE. MQTT + OTA повністю працюють. MQTT WebUI сторінка додана.
  WiFi PS bug виправлений (DS18B20 більше не спамить esp_wifi_set_ps). Milestone M2 ДОСЯГНУТО.
- 2026-02-17 — Thermostat: температура публікується в SharedState, settings синхронізуються з WebUI.
  Gauge замінено на value widget.
- 2026-02-17 — Driver manifests, DriverManifestValidator, cross-валідація,
  bindings page, board.json→gpio_outputs, C++ HAL оновлений. 66 тестів зелені.
- 2026-02-17 — HTMLGenerator видалений. WebUI статичний (data/www/).
- 2026-02-17 — Phase 5c DONE. Milestone M1 ДОСЯГНУТО.
- 2026-02-16 — Створено
