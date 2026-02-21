# ModESP v4 — Дорожня карта розвитку

> Останнє оновлення: 2026-02-20
> Версія прошивки: 4.0.0
> Платформа: ESP32-WROOM-32, ESP-IDF v5.5, C++17 + ETL

---

## Загальне бачення

ModESP v4 — модульна платформа для промислових ESP32-контролерів
холодильного обладнання. Замінює дорогі контролери Danfoss/Dixell
дешевим ESP32 з професійною архітектурою, автогенерованим WebUI
та стандартними протоколами (MQTT, Modbus).

**Цільова аудиторія:** OEM виробники (UBC Group, Modern Expo),
сервісні інженери, інтегратори холодильного обладнання.

---

## ✅ Завершені фази

### Phase 1 — Ядро (завершено)
- ModuleManager з пріоритетами та lifecycle
- SharedState (thread-safe, zero-heap, etl::unordered_map<96>)
- EventBus (etl::message_bus<24>, pub/sub)
- Базові типи (StateKey<24>, StateValue variant)
- App singleton з main loop

### Phase 2 — Системні сервіси (завершено)
- ErrorService — збір помилок, Safe Mode
- WatchdogService — heartbeat модулів, auto-restart
- LoggerService — ring buffer, рівні логування
- SystemMonitor — RAM, uptime, boot reason
- ConfigService — board.json + bindings.json з LittleFS
- NVS Helper — persist критичних налаштувань

### Phase 3 — HAL + Драйвери (завершено)
- HAL абстракція GPIO
- DriverManager — створення драйверів з bindings.json
- DS18B20 драйвер (OneWire, retry, validation, CRC)
- Relay драйвер (min on/off time protection)

### Phase 4 — Thermostat модуль (завершено)
- On/Off регулювання з гістерезисом
- Публікація стану через SharedState
- (Phase 9.1) Рефакторинг: працює через Equipment Manager, req.* замість direct HAL

### Phase 5a — WiFi + HTTP + WebSocket (завершено)
- WiFiService — AP mode (ModESP-XXXX), credentials в NVS
- HttpService — REST API (11 endpoints), LittleFS static files
- WsService — real-time state broadcast, heartbeat PING/PONG
- Повний API: /api/state, /api/ui, /api/settings, /api/modules...

### Phase 5b — UI Generation Pipeline (завершено)
- Manifest-driven architecture (module manifest.json → all artifacts)
- Python генератор generate_ui.py (820 рядків, рефакторинг з 1216)
- Артефакти: ui.json, state_meta.h, mqtt_topics.h, display_screens.h
- Svelte WebUI (webui/) замінив inline HTML генерацію

---

## ✅ Phase 5c — Quality & Documentation (завершено)
**Мета:** Стабілізація, тести, документація перед новими фічами.

- [x] Bugfix: WsService double-init warning
- [x] Рефакторинг: JS/CSS з Python strings → template файли
- [x] Оновити next_prompt.md до актуального стану
- [x] Доповнити manifest spec (Module + Bindings секції)
- [x] manifest_version поле в усіх маніфестах
- [x] Pytest suite для генератора (56 тестів, всі зелені)
- [x] Рефакторинг generate_ui.py для testability

**Результат:** `pytest tools/tests/ -v` — 56 passed (0.29s).
Manifest spec покриває Board, Driver, Module, Bindings + Validation Summary.

---

## ✅ Phase 6 — MQTT + OTA (завершено)
**Мета:** Віддалене управління та оновлення прошивки.

- [x] MqttService (528 рядків): NVS config, esp_mqtt_client, LWT, publish/subscribe
- [x] MQTT TLS: підключення до HiveMQ Cloud (mqtts://:8883, CA bundle)
- [x] MQTT HTTP API: GET/POST /api/mqtt з CORS
- [x] MQTT WebUI: сторінка конфігурації (статус + форма налаштувань)
- [x] mqtt_topics.h + state_meta.h генеруються з маніфестів
- [x] OTA upload: POST /api/ota (chunked 1KB), CRC validation
- [x] OTA rollback: esp_ota_mark_app_valid на першому boot
- [x] OTA WebUI: firmware_upload widget з progress bar
- [x] Dual-partition: factory 1.5MB + ota_0 1.5MB
- [x] SNTP: часовий пояс EET-2EEST (Україна)

**Не реалізовано (перенесено):**
- [ ] MQTT auto-discovery (Home Assistant)

---

## ✅ Phase 6.5 — Auto-persist settings (завершено)
**Мета:** Автоматичне збереження налаштувань в NVS.

**Auto-persist — РЕАЛІЗОВАНО:**
- [x] PersistService: boot restore всіх persist:true ключів з NVS → SharedState
- [x] SharedState.set() → persist callback ПОЗА mutex → dirty flag → debounce 5s → NVS flush
- [x] state_meta.h: persist + default_val поля (генеруються з manifest)
- [x] POST /api/settings: валідація через state_meta (writable check, min/max clamp)
- [x] Модулі НЕ працюють з NVS напряму — thermostat читає з SharedState
- [x] NVS key strategy: djb2 hash від state key name ("s" + 7 hex chars)

**DataLogger module (окремий модуль, перенесено на Phase 9+):**
- [ ] Підписується через inputs на потрібні ключі
- [ ] Записує з інтервалом в circular buffer на LittleFS
- [ ] GET /api/log — віддає дані для графіків
- [ ] Manifest-driven: що логувати описано в inputs модуля

## ✅ Phase 7a — Svelte WebUI: Scaffold + Dashboard + WebSocket (завершено)
**Мета:** Професійний веб-інтерфейс замість inline HTML.

- [x] Svelte 4 проект в webui/, Rollup build pipeline
- [x] WebSocket store (reactive state, auto-reconnect з backoff)
- [x] UI config store (завантаження /api/ui)
- [x] Tile-based Dashboard (CSS Grid, temperature color zones, compressor pulse animation)
- [x] Layout: sidebar (desktop, collapsible on tablet) + bottom tabs (mobile)
- [x] Lucide SVG icons (inline, no dependency)
- [x] 14 widget components: value, slider, number_input, indicator, status_text,
      toggle, text_input, password_input, button, firmware_upload, wifi_save,
      mqtt_save, time_save, datetime_input
- [x] DynamicPage: renders any page from ui.json dynamically
- [x] Dark theme (промислове обладнання)
- [x] Responsive: desktop (sidebar + 2-col grid), tablet (collapsed sidebar), mobile (bottom tabs + 1-col)
- [x] Build pipeline: npm run build → gzip → npm run deploy → data/www/
- [x] Bundle size: **17KB gzipped** (target <60KB)

**Структура:**
```
webui/
├── src/
│   ├── App.svelte              # Router + layout shell
│   ├── main.js                 # Entry point
│   ├── stores/
│   │   ├── state.js            # WebSocket → Svelte writable store
│   │   └── ui.js               # UI config store (from /api/ui)
│   ├── lib/
│   │   ├── websocket.js        # Auto-reconnect WS client
│   │   ├── api.js              # fetch wrappers (GET/POST/Upload)
│   │   └── icons.js            # Inline Lucide SVG paths
│   ├── components/
│   │   ├── Layout.svelte       # Sidebar + header + content
│   │   ├── Card.svelte         # Collapsible card container
│   │   ├── Icon.svelte         # SVG icon component
│   │   ├── WidgetRenderer.svelte # Widget type → component mapper
│   │   └── widgets/            # 14 widget components
│   └── pages/
│       ├── Dashboard.svelte    # Tile-based dashboard
│       └── DynamicPage.svelte  # Generic page renderer
├── scripts/deploy.js           # Gzip + copy to data/www/
├── package.json
└── rollup.config.js
```

## ✅ Phase 9.5 — Audit + Bugfixes (завершено)
**Мета:** Стабілізація після Phase 9.

- [x] Architecture review + Business logic review vs spec_v3
- [x] Independent bug verification — 27 bugs found, 14 fixed
- [x] 10 critical + 7 quick-win audit fixes
- [x] NVS hash-based keys migration, POST type detection fix

---

## ✅ Phase 10.5 — Features System + Select Widgets (завершено)
**Мета:** Прогресивне розкриття функціональності.

- [x] Features в маніфестах (thermostat/defrost/protection)
- [x] Constraints (enum_filter) для enum settings
- [x] Select widgets з options (value+label)
- [x] FeatureResolver + FeaturesConfigGenerator (5-й артефакт)
- [x] has_feature() в BaseModule + guards в C++ модулях
- [x] SelectWidget.svelte + disabled state в WebUI
- [x] 209 pytest tests green

---

## 📋 Заплановані фази

### Phase 7b-c — Svelte WebUI: Polish
**Мета:** Графіки, анімації, PWA.
**Пріоритет:** СЕРЕДНІЙ.

- [ ] Графіки температури (Phase 7c)
- [ ] Animations/transitions
- [ ] Light theme toggle
- [ ] i18n
- [ ] PWA / Service Worker
- [ ] CMake інтеграція (auto-build Svelte перед idf.py build)

### Phase 8 — LCD/OLED Display
**Мета:** Локальний інтерфейс без WiFi для сервісних інженерів.
**Пріоритет:** СЕРЕДНІЙ — потрібний для standalone контролерів.

- [ ] Display Service (абстракція SSD1306/SH1106/ST7920)
- [ ] display_screens.h вже генерується — підключити до рендеру
- [ ] Навігація: кнопки або енкодер
- [ ] Екрани: Dashboard, Settings, Alarms, System Info
- [ ] Menu builder з manifest display section

**Залежності:** Phase 5c (display_screens.h)
**Оцінка:** 3-4 сесії

### Phase 9 — Equipment Layer + Промислові модулі
**Мета:** Промисловий контролер холодильного обладнання (за spec_v3).
**Пріоритет:** НАЙВИЩИЙ для комерціалізації.

**Архітектурне рішення (2026-02-18): Equipment Layer**

Єдиний Equipment Manager (EM) володіє всіма drivers (сенсори, реле).
Бізнес-модулі (Thermostat, Defrost, Protection) працюють ТІЛЬКИ через SharedState:
читають стани обладнання, публікують requests. EM арбітрує requests
з пріоритетами та інтерлоками, керує relay.

```
Protection > Defrost > Thermostat (пріоритет арбітражу)
```

Інтерлоки (hardcoded в EM):
- Тен і компресор НІКОЛИ одночасно (dFT=1)
- Клапан ГГ відкривається ДО компресора (dFT=2)
- Protection lockout = все вимкнено

**Порядок реалізації:**
- [x] **9.1 Equipment Manager** — новий модуль (priority=0), єдиний власник HAL. (DONE — 2026-02-18)
  Читає сенсори → equipment.*, читає req.* → relay. Арбітраж, інтерлоки.
  Thermostat рефакторинг (req.compressor замість direct relay).
  bindings.json: module="equipment". Cross-module widget keys в generate_ui.py.
  
- [x] **9.2 Thermostat v2** — повна логіка зі spec_v3 (§2). (DONE — 2026-02-18)
  State machine: STARTUP → IDLE → COOLING → SAFETY_RUN.
  Асиметричний диференціал (ON: T>=SP+rd, OFF: T<=SP), вент. випарника (3 режими FAn),
  вент. конденсатора (затримка COd), Safety Run, 11 persist параметрів, 18 state keys.
  
- [x] **9.3 Protection** — захист і аварії (spec_v3 §2.7 + розширення). (DONE — 2026-02-18)
  5 alarm monitors: HAL, LAL, ERR1, ERR2, Door. Delayed alarms (dAd хв), defrost blocking.
  Auto-clear + manual reset (protection.reset_alarms). 5 persist params, 14 state keys.
  protection.lockout reserved (always false). Priority HIGH(1) — runs before Thermostat.
  
- [x] **9.4 Defrost** — цикл розморозки (spec_v3 §3). (DONE — 2026-02-18)
  7-phase state machine: IDLE→STABILIZE→VALVE_OPEN→ACTIVE→EQUALIZE→DRIP→FAD.
  3 типи: зупинка (dFT=0) / тен (dFT=1) / гарячий газ (dFT=2, 7 фаз).
  4 ініціації: таймер (dit) / demand (dSS) / комбінований / ручний.
  13 persist params + 2 runtime persist (interval_timer, defrost_count).
  27 state keys, 10 MQTT publish. Generator fix: read-only persist → state_meta.h.

**Базовий документ:** docs/controller_spec_v3.docx (10K chars, 19 таблиць параметрів)

**Залежності:** Phase 6.5 (auto-persist), Phase 7a (WebUI для відображення)
**Оцінка:** 6-8 сесій

### Phase 11 — Multi-sensor + PID
**Мета:** Підтримка складних конфігурацій обладнання.
**Пріоритет:** СЕРЕДНІЙ.

- [ ] Кілька DS18B20 на одній шині (address-based binding)
- [ ] NTC сенсор (аналоговий, через ADC)
- [ ] PID регулювання (замість on/off гістерезису)
- [ ] Pressure sensor підтримка
- [ ] Модуль Evaporator (fan + defrost + sensor group)

**Залежності:** Phase 3 (HAL), Phase 9 (defrost)
**Оцінка:** 3-4 сесії

### Phase 12 — NTP + mDNS + Cloud
**Мета:** Повноцінна мережева інтеграція.
**Пріоритет:** СЕРЕДНІЙ.

Вже реалізовано:
- ✅ WiFi Station mode (підключення до роутера, credentials з NVS)
- ✅ AP fallback якщо Station не вдалося
- ✅ WiFi scan через /api/wifi/scan
- ✅ SNTP (NTP) працює — TZ=EET-2EEST (Україна), синхронізація часу при boot

Залишилось:
- [ ] mDNS (modesp-xxxx.local)
- [ ] Cloud connector (опціонально: MQTT broker → dashboard)

**Залежності:** Phase 5a (WiFi AP), Phase 6 (MQTT)
**Оцінка:** 2-3 сесії

### Phase 13 — Modbus + Industrial Protocols
**Мета:** Інтеграція з промисловими системами (BMS, SCADA).
**Пріоритет:** НИЗЬКИЙ (для великих інсталяцій).

- [ ] Modbus RTU (RS485) — slave mode
- [ ] Modbus TCP — slave mode
- [ ] Register map з manifest (auto-generated)
- [ ] Integration з Danfoss AK-SM 800, Carel pCO

**Залежності:** Phase 10 (multi-sensor)
**Оцінка:** 3-4 сесії

---

## 🗓️ Візуальна дорожня карта

```
2026 Q1                    Q2                      Q3
──────────────────────────────────────────────────────────────
 ✅ Phase 1-6.5            ✅ Phase 9      📋 Phase 7b-c
 (ядро, HAL, WiFi,          Equipment      WebUI polish
  HTTP, WS, MQTT, OTA,      Thermostat     (charts, PWA)
  persist, Svelte UI)        Defrost
                             Protection    📋 Phase 8
                           ✅ Phase 9.5    LCD/OLED
                             Audit+Bugs
                           ✅ Phase 10.5   📋 Phase 11
                             Features       Multi-sensor
──────────────────────────────────────────────────────────────
                                            2027 Q1
                                            📋 Phase 12-13
                                             NTP/mDNS, Modbus
```

---

## 🎯 Milestones

### M1: "Лабораторний прототип" ✅ ДОСЯГНУТО
**Phases 1-5c завершені.**
ESP32 з DS18B20 + реле, WiFi STA/AP, WebUI, REST API, WebSocket.
56 тестів генератора зелені. Manifest standard задокументований.

### M2: "Connected Controller" (Phase 6) ✅ ДОСЯГНУТО
Стабільна прошивка з MQTT, OTA оновленням.
Можна розгорнути на реальному обладнанні з віддаленим моніторингом.

### M3: "Production Ready" (Phase 9 + 9.5 + 10.5) ✅ ДОСЯГНУТО
Промислові модулі, features system, select widgets, аудит + bugfixes.
Готовий для демонстрації OEM виробникам (UBC, Modern Expo).

### M4: "Industrial Grade" (Phase 11 + 12 + 13)
Multi-sensor, PID, Modbus, Cloud.
Повноцінна заміна Danfoss ERC/AK контролерів.

---

## 📝 Принципи розвитку

1. **Manifest-first** — нова функціональність починається з manifest schema
2. **Zero heap в hot path** — ETL замість STL, constexpr де можливо
3. **Генератор як bridge** — Python зв'язує manifest з C++ та Web
4. **Документація = частина продукту** — CLAUDE.md та docs/ оновлюються з кодом
5. **Тести перед фічами** — Phase 5c (тести) перед Phase 6 (MQTT)

---

## Changelog
- v14.0 (2026-02-20) — Phase 10.5 DONE: Features System + Select Widgets. Phase 9.5 DONE: Audit+Bugfixes.
  Milestone M3 ДОСЯГНУТО. Phase numbering: old 10→11, 11→12, 12→13. 209 tests, 5 artifacts.
- v12.0 (2026-02-18) — Phase 9.4 DONE: Defrost module (7-phase state machine, 3 types, 4 initiations,
  13 persist params + 2 runtime persist). Phase 9 COMPLETE — 4 industrial modules. 69 state keys.
- v11.0 (2026-02-18) — Phase 9.3 DONE: Protection module (5 alarm monitors, dAd delay, defrost blocking,
  auto-clear + manual reset, 5 persist params, 14 state keys). 3 modules, 42 state keys total.
- v10.0 (2026-02-18) — Phase 9.2 DONE: Thermostat v2 (asymmetric differential, state machine,
  fan control, Safety Run, 11 persist params, 18 state keys). hysteresis→differential rename.
- v9.0 (2026-02-18) — Phase 9.1 DONE: Equipment Manager (HAL owner, arbitration, interlocks).
  Thermostat рефакторинг: req.* замість direct relay. Cross-module widget keys в генераторі.
- v8.0 (2026-02-17) — Phase 7a DONE: Svelte WebUI (14 widgets, Dashboard, sidebar/tabs, 17KB gzipped).
  Phase 7 розбито на 7a (scaffold, done) та 7b-c (charts, animations, PWA — pending).
- v7.0 (2026-02-17) — Phase 6.5 DONE: PersistService, SharedState persist callback, state_meta validation.
  Auto-persist = framework-level NVS. DataLogger перенесено на Phase 9+.
- v6.0 (2026-02-17) — MQTT TLS + SNTP підтверджені boot log. Phase 6.5 додано (auto-persist + DataLogger).
  Архітектурні рішення: mutual exclusion через inputs, timers per-module, auto-persist framework-level.
  Phase 9 оновлено: prerequisites, architectural decisions.
- v5.0 (2026-02-17) — Phase 6 DONE. MQTT повністю реалізований (528 рядків C++), OTA працює.
  MQTT WebUI page додана. Milestone M2 ДОСЯГНУТО. Наступний: Phase 7 (Svelte WebUI).
- v4.0 (2026-02-17) — Синхронізовано з реальністю (boot log підтвердження).
  WiFi STA, OTA partition, MQTT каркас позначені як частково реалізовані.
  Phase 5c DONE. Milestone M1 ДОСЯГНУТО.
- v3.0 (2026-02-16) — Повне перезаписування. Актуальний стан Phase 1-5b.
  Додано Svelte WebUI (Phase 7), milestones, візуальну карту.
- v2.0 — Phase 5a-5b додані
- v1.0 — Початковий план (Phase 1-5)
