# ModESP v4 — План дій

> Точка входу в проект. Відкривай на початку кожної робочої сесії.

---

## Де ми зараз

**Milestone M3: "Production Ready" — ДОСЯГНУТО (2026-02-20).**

Повністю працюючий контролер холодильної камери:
- 4 бізнес-модулі (Equipment Manager, Thermostat v2, Defrost 7-phase, Protection 5 alarms)
- 3 драйвери (DS18B20, Relay, Digital Input)
- Features System — UI показує тільки підключене обладнання
- Svelte WebUI (15 widgets, Dashboard, dark theme, 17KB gzipped)
- MQTT TLS, OTA з rollback, auto-persist NVS
- 206 pytest тестів зелені, генератор → 5 артефактів

**Поточна фаза: Phase 11a — Danfoss-level Improvements — DONE.**

---

## Відкриті проблеми

### Bugs (оновлено 2026-02-21)

| ID | Sev | Опис | Статус |
|----|-----|------|--------|
| ~~BUG-005~~ | ~~2~~ | ~~Thermostat state inconsistency при defrost~~ | **FIXED** |
| ~~BUG-022~~ | ~~1~~ | ~~Компресор не стартує після відтайки (COOLING без request)~~ | **FIXED** (force state→idle після defrost) |
| ~~BUG-006~~ | ~~2~~ | ~~enter_phase() без re-entry guard~~ | **FIXED** (if phase==p return) |
| ~~BUG-016~~ | ~~3~~ | ~~String-based phase compare~~ | **FIXED** |
| ~~BUG-013~~ | ~~3~~ | ~~read_float/bool/int дублюються в 3 модулях~~ | **FIXED** (перенесено в BaseModule) |
| ~~BUG-014~~ | ~~3~~ | ~~No immediate NVS flush before restart~~ | **FIXED** (flush_now() + виклик перед esp_restart) |
| ~~BUG-018~~ | ~~3~~ | ~~state_set() return value ignored~~ | **FIXED** (ESP_LOGE + set_failures_ counter) |
| ~~BUG-021~~ | ~~2~~ | ~~WS client_fds_ race condition~~ | **FIXED** (StaticSemaphore mutex) |

### OPEN audit items (оновлено 2026-02-21)

**Пріоритет 2 (перед production):**
| ID | Опис | Примітка |
|----|------|----------|
| ~~AUDIT-011~~ | ~~Post-defrost alarm suppression timer (30-60 хв)~~ | **FIXED** (Phase 11a) |
| AUDIT-012 | Separate alarm_delay для HAL і LAL | |
| AUDIT-013 | Door sensor hardcoded false в equipment_module | digital_input driver є, але не bind-иться |
| ~~AUDIT-019~~ | ~~protection.reset_alarms — немає UI widget~~ | **FIXED** — кнопка в manifest UI |
| ~~AUDIT-020~~ | ~~WS broadcast: malloc в hot path~~ | **CLOSED** — broadcast = stack buffer |
| ~~AUDIT-021~~ | ~~Серіалізаційний буфер 3072~~ | **FIXED** — збільшено до 4096 (запас до ~115 ключів) |

**Пріоритет 3 (commercial viability):**
| ID | Опис |
|----|------|
| AUDIT-030 | HTTP автентифікація (Basic Auth) |
| AUDIT-031 | HP/LP pressure switch digital inputs |
| AUDIT-032 | Scheduled defrost (за часом доби) |
| AUDIT-033 | Alarm relay output для BMS |
| AUDIT-034 | Password protection для параметрів |
| AUDIT-035 | °C/°F вибір одиниць |
| AUDIT-036 | Compressor lockout після N коротких циклів |
| AUDIT-037 | Cache-Control headers для static files |

### Перенесені задачі
- MQTT auto-discovery (Home Assistant) — не критично
- DataLogger module (circular buffer на LittleFS, GET /api/log для графіків)


---

## Що робити далі — кандидати

### Варіант A: WebUI Polish (Phase 7b-c)
Графіки температури, PWA, анімації, i18n, light theme.
**Цінність:** візуально вражає на демо OEM. **Складність:** середня.

### Варіант B: LCD/OLED Display (Phase 8)
Локальний інтерфейс без WiFi для сервісних інженерів.
display_screens.h вже генерується — залишилось підключити рендер.
**Цінність:** standalone контролер. **Складність:** середня (3-4 сесії).

### Варіант C: Bugfix + Hardening
Закрити OPEN audit items P2 (AUDIT-012, 013).
**Цінність:** стабільність. **Складність:** низька (1-2 сесії).

### Варіант D: Multi-sensor + PID (Phase 11b)
NTC через ADC, кілька DS18B20, PID замість on/off.
**Цінність:** розширення hardware. **Складність:** висока.

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

**Детальна історія:** docs/06_roadmap.md, docs/BUGFIXES_VERIFIED.md

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
