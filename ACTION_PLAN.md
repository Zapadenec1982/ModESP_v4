# ModESP v4 — План дій

> Точка входу в проект. Відкривай на початку кожної робочої сесії.

---

## Де ми зараз

**Milestone M3: "Production Ready" — ДОСЯГНУТО (2026-02-20).**

Повністю працюючий контролер холодильної камери:
- 4 бізнес-модулі (Equipment Manager, Thermostat v2, Defrost 7-phase, Protection 5 alarms)
- 6 драйверів (DS18B20, Relay, Digital Input, NTC, PCF8574 Relay, PCF8574 Input)
- Features System — UI показує тільки підключене обладнання
- Runtime UI visibility — visible_when + per-option disabled
- Svelte WebUI (24 widgets, Dashboard, dark/light theme, i18n UA/EN, 44KB gzipped)
- MQTT TLS, OTA з rollback, auto-persist NVS
- 264 pytest + 90 host C++ (doctest) тестів зелені, генератор → 5 артефактів
- KC868-A6 board support (Phase 12a) — I2C PCF8574 expander, pcf8574_relay + pcf8574_input drivers
- Heap optimization: NVS batch API, WS broadcast 3000ms, float rounding, heap guard 40KB

**Поточна фаза: Phase 12a — KC868-A6 board support + heap optimization — DONE.**

---

## Відкриті проблеми

### Bugs

| ID | Опис | Статус |
|----|------|--------|
| BUG-025 | MQTT: таймери (comp_on/off_time, phase/interval_timer) спамлять через version bump щосекунди. Потрібно точкове рішення на рівні MQTT (per-key publish rate або виключення), без зміни C++ логіки state_set(). Зараз тимчасово прибрані з mqtt.publish в маніфестах. | OPEN |

Попередні 8 багів виправлені (BUG-005/006/013/014/016/018/021/022).
Деталі: `docs/archive/BUGFIXES_VERIFIED.md`

### OPEN audit items

**Пріоритет 2:** Всі закриті (AUDIT-011/012/013/019/020/021).

**Пріоритет 3 (commercial viability):**
| ID | Опис | Статус |
|----|------|--------|
| AUDIT-030 | HTTP автентифікація (Basic Auth) | |
| AUDIT-031 | HP/LP pressure switch digital inputs | |
| AUDIT-032 | Scheduled defrost (за часом доби) | |
| AUDIT-033 | Alarm relay output для BMS | |
| AUDIT-034 | Password protection для параметрів | |
| AUDIT-035 | °C/°F вибір одиниць | |

### Перенесені задачі
- MQTT auto-discovery (Home Assistant) — не критично


---

## Що робити далі — кандидати

### ~~Варіант A: WebUI Polish (Phase 7b-c)~~ — ЧАСТКОВО DONE
~~Графіки температури~~, ~~анімації~~, ~~i18n~~, ~~light theme~~. Залишилось: PWA, CMake інтеграція.
**Статус:** Theme toggle, i18n UA/EN, transitions — реалізовані. Bundle 43.7KB gz.

### Варіант B: LCD/OLED Display (Phase 8)
Локальний інтерфейс без WiFi для сервісних інженерів.
display_screens.h вже генерується — залишилось підключити рендер.
**Цінність:** standalone контролер. **Складність:** середня (3-4 сесії).

### Варіант C: Hardening
Закрити OPEN audit items P3 (AUDIT-030..035).
**Цінність:** commercial viability. **Складність:** середня (2-3 сесії).

### ~~Варіант D: Multi-sensor + PID (Phase 11b)~~ — DONE (без PID)
NTC через ADC, кілька DS18B20, DigitalInput driver.
**Статус:** Multi DS18B20 (MATCH_ROM) + NTC/ADC + DigitalInput + PCF8574 реалізовані (6 драйверів). PID — пізніше.

### Майбутні покращення (Danfoss-inspired, після Phase 11a)
- **Fan cycling** (duty cycle коли компресор OFF) — ERC 213
- **Adaptive Defrost** (пропуск відтайки за аналізом випарника) — AK-CC 550
- **Condenser overheat protection** (alarm + lockout при перегріві конденсатора) — ERC 214
- **Pump Down** перед defrost (спорожнення випарника від фреону) — AK-CC 550
- **Coordinated defrost** через MQTT (master/slave) — EKC 202D
- **Drip tray heater** (нагрів піддону після defrost) — AK-CC 550

---

## Завершені фази (стисло)

| Фаза | Що | Коли |
|------|----|------|
| 1-5c | Ядро, HAL, WiFi, HTTP, WS, UI generation, тести | 02-12..02-17 |
| 6 | MQTT TLS + OTA з rollback. **M2 ДОСЯГНУТО** | 02-17 |
| 6.5 | Auto-persist settings (NVS, PersistService) | 02-17 |
| 7a | Svelte WebUI (15 widgets, Dashboard, 17KB gz) | 02-17 |
| 9 | Equipment Manager + Thermostat v2 + Defrost + Protection | 02-18 |
| 9.5 | Audit (3 агенти) + Bugfixes (14/27 fixed) | 02-19..02-20 |
| 10.5 | Features System + Select Widgets (209 тестів) | 02-20 |
| **11a** | **Danfoss-level: Night Setback, Post-defrost, Display** | **DONE 02-21** |
| **11b** | **Multi DS18B20 (MATCH_ROM) + NTC/ADC + DigitalInput** | **DONE 02-22** |
| **13a** | **Runtime UI visibility (visible_when + disabled options)** | **DONE 02-23** |
| **14** | **DataLogger + ChartWidget (LittleFS, SVG, streaming JSON)** | **DONE 02-24** |
| **14a** | **Multi-channel DataLogger (3ch, event list, CSV export)** | **DONE 02-24** |
| **14b** | **6-channel dynamic DataLogger + ChartWidget** | **DONE 02-24** |
| **7b-c** | **WebUI Polish: theme, i18n, animations, Catmull-Rom** | **DONE 02-24** |
| **12a** | **KC868-A6: I2C PCF8574 relay + input drivers** | **DONE 03-01** |

**Детальна історія:** docs/06_roadmap.md, docs/archive/BUGFIXES_VERIFIED.md

---

## Архітектурні рішення (діючі)

| Тема | Рішення |
|------|---------|
| HAL ownership | Єдиний Equipment Manager володіє drivers. Модулі через SharedState req.* |
| Арбітраж | Protection > Defrost > Thermostat. Інтерлоки в EM |
| Persist | Auto-persist framework-level (SharedState → NVS, debounce 5s) |
| Таймери | Per-module (millis + FSM в on_update) |
| Features | Static rebuild: bindings → FeatureResolver → features_config.h при boot |
| UI | Manifest → generate_ui.py → ui.json → Svelte WebUI runtime |

---

## Як працювати з Claude Code

1. Claude Code читає `CLAUDE.md` (автоматично)
2. Ти кажеш що хочеш зробити (або даєш CLAUDE_TASK.md)
3. Claude Code працює, ти перевіряєш

**Після кожної сесії — Claude Code ОБОВ'ЯЗКОВО:**
- Оновлює цей файл (зняти галочки, додати нове)
- Оновлює CLAUDE.md якщо змінилась архітектура
- Фіксує нові баги в OPEN таблицях вище

**Якщо загубився:**
- `ACTION_PLAN.md` → що робити
- `CLAUDE.md` → як працює проект
- `docs/06_roadmap.md` → куди йдемо
- `docs/10_manifest_standard.md` → як писати маніфести

---

## Changelog

- 2026-03-01 — Phase 12a DONE: KC868-A6 board support (pcf8574_relay + pcf8574_input). defrost_relay merger. NVS batch API. Heap optimization (WS guard 40KB, float rounding, broadcast 3000ms). 6 drivers, 53 STATE_META, 52 MQTT sub.
- 2026-02-24 — Phase 14b: 6-channel dynamic DataLogger (TempRecord 16 bytes, ch[6], ChannelDef table), dynamic ChartWidget (PALETTE, toggles, setpoint dual-mode). +log_setpoint, +log_humidity. 97 state keys, 264 тестів.
- 2026-02-24 — Phase 14a: Multi-channel DataLogger (air+evap+cond, TempRecord 12 bytes, JSON v2), event text list, CSV export. Equipment +has_cond_temp. Fix Cache-Control no-store. 95 state keys, 207 тестів.
- 2026-02-24 — Phase 14 DONE: DataLogger module (append+rotate LittleFS, streaming chunked JSON, 10 event types) + ChartWidget (SVG polyline, downsample, zones, tooltip). 92 state keys, 207 тестів.
- 2026-02-23 — Phase 13a DONE: Runtime UI visibility. visible_when на cards/widgets, requires_state на select options, equipment.has_* keys (+3), isVisible() Svelte utility. 84 state keys, 178 тестів.
- 2026-02-23 — Phase 11b COMPLETE: SEARCH_ROM (Maxim AN187), GET /api/onewire/scan endpoint, WebUI OneWire Discovery (scan + assign in BindingsEditor). HttpService: set_hal() for scan. 206 тестів.
- 2026-02-22 — Phase 11b: Multi DS18B20 (MATCH_ROM), NTC/ADC driver, DigitalInput C++ driver. HAL: GpioInputConfig, AdcChannelConfig, Binding.address. Equipment: condenser_temp + door_contact. 81 state keys, 206 тестів.
- 2026-02-21 — Phase 11a DONE: Night Setback (4 modes: off/schedule/DI/manual), Post-defrost alarm suppression (0-120 хв), Display during defrost (real/frozen/-d-). Equipment: night_input role. 80 state keys, 206 тестів, WebUI deployed.
- 2026-02-21 — BUG-022 FIXED: компресор не стартує після відтайки. Причина: thermostat залишався в COOLING з compressor_on_=false, enter_state(COOLING) блокувався re-entry guard. Фікс: force state→IDLE після defrost.
- 2026-02-21 — Bugfix batch: BUG-006 (re-entry guard), BUG-021 (WS mutex), BUG-014 (flush_now), BUG-013 (read helpers→BaseModule), BUG-018 (ESP_LOGE+counter), AUDIT-019 (reset_alarms button), AUDIT-021 (buffer 4096). 0 OPEN bugs, 206 тестів зелені.
- 2026-02-21 — Аудит багів: всі OPEN bugs перевірені (5 OPEN, AUDIT-020 CLOSED). Фікси сесії: interlock ordering (anti-short-cycle BEFORE interlocks), defrost toggle (start/stop), has_feature guards removed, constraints removed, TimezoneSelect, duration format HH:MM:SS, HTTP max_open_sockets=4.
- 2026-02-21 — Повне перезаписування: стиснуто завершені фази, видалено дублювання, структуровано відкриті проблеми.
- 2026-02-20 — Phase 10.5 DONE: Features System + Select Widgets. M3 ДОСЯГНУТО. 209 тестів.
- 2026-02-20 — Audit: 10 critical + 7 quick-win fixes. Bugfixes: 14/27 fixed.
- 2026-02-18 — Phase 9 COMPLETE: 4 промислові модулі. M2 вже досягнуто (Phase 6).
- 2026-02-17 — Phase 6+6.5+7a DONE: MQTT, OTA, persist, Svelte WebUI.
- 2026-02-16 — Створено.
