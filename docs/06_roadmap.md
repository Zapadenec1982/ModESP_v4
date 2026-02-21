# ModESP v4 — Дорожня карта розвитку

> Останнє оновлення: 2026-02-21
> Платформа: ESP32-WROOM-32, ESP-IDF v5.5, C++17 + ETL

---

## Загальне бачення

ModESP v4 — модульна платформа для промислових ESP32-контролерів
холодильного обладнання. Замінює контролери Danfoss/Dixell дешевим
ESP32 з manifest-driven архітектурою, автогенерованим WebUI
та стандартними протоколами (MQTT, Modbus).

**Цільова аудиторія:** OEM виробники (UBC Group, Modern Expo),
сервісні інженери, інтегратори.

---

## ✅ Завершені фази

### Phase 1-5c — Фундамент (завершено 2026-02-17)

Ядро (ModuleManager, SharedState, EventBus), системні сервіси
(Error, Watchdog, Logger, NVS), HAL + драйвери (DS18B20, Relay),
WiFi STA/AP, HTTP REST API (12 endpoints), WebSocket broadcast,
UI generation pipeline (manifest → 4 артефакти), pytest suite.

**Milestone M1: "Лабораторний прототип" — ДОСЯГНУТО.**

### Phase 6 — MQTT + OTA (завершено 2026-02-17)

MQTT TLS (528 рядків, esp_mqtt_client, HiveMQ Cloud), OTA upload
з dual-partition rollback, SNTP (EET-2EEST), WebUI сторінки для MQTT та OTA.

**Milestone M2: "Connected Controller" — ДОСЯГНУТО.**

### Phase 6.5 — Auto-persist settings (завершено 2026-02-17)

PersistService: boot restore NVS → SharedState, debounce 5s flush,
NVS hash-based keys (djb2). POST /api/settings з state_meta валідацією.

### Phase 7a — Svelte WebUI (завершено 2026-02-17)

Svelte 4 + Rollup, 15 widget components, tile-based Dashboard,
sidebar/bottom tabs, dark theme, 17KB gzipped. Deploy: webui/ → data/www/.

### Phase 9 — Equipment Layer + Промислові модулі (завершено 2026-02-18)

**Equipment Manager** — єдиний HAL owner з арбітражем (Protection > Defrost > Thermostat)
та інтерлоками (тен↔компресор, тен↔клапан ГГ). Compressor anti-short-cycle на output level.

**Thermostat v2** — асиметричний диференціал, state machine (STARTUP→IDLE→COOLING→SAFETY_RUN),
вент. випарника (3 режими), вент. конденсатора (затримка COd), Safety Run, 11 persist params.

**Defrost** — 7-фазна FSM, 3 типи (зупинка/тен/гарячий газ), 4 ініціації
(таймер/demand/комбінований/ручний), 13 persist params + 2 runtime persist.

**Protection** — 5 alarm monitors (HAL, LAL, ERR1, ERR2, Door), delayed alarms (dAd),
defrost blocking, auto-clear + manual reset, 5 persist params.

### Phase 9.5 — Audit + Bugfixes (завершено 2026-02-20)

Architecture review + business logic review vs spec_v3.
27 bugs знайдено (незалежна верифікація), 14 виправлено.
10 critical audit fixes (relay min_switch, EM anti-short-cycle, JSON escaping,
WebUI alarm banner, Dashboard). 7 quick-wins (manifest ranges, CORS, security).
Деталі: `docs/BUGFIXES_VERIFIED.md`

### Phase 10.5 — Features System + Select Widgets (завершено 2026-02-20)

Features в маніфестах (requires_roles → bindings → active/inactive).
Constraints (enum_filter) для dropdown options. Select widgets з людськими labels.
FeatureResolver + FeaturesConfigGenerator (5-й артефакт). has_feature() в C++ модулях.
SelectWidget.svelte + disabled state в WebUI. 209 pytest тестів.

**Milestone M3: "Production Ready" — ДОСЯГНУТО.**

---

### Phase 11a — Danfoss-level Improvements (завершено 2026-02-21)

Алгоритми на рівні топових контролерів Danfoss EKC 202/ERC 213/AK-CC 550:
- **Night Setback** — 4 режими (off/schedule/DI/manual), зміщення setpoint, SNTP schedule
- **Post-defrost alarm suppression** — блокування HAL alarm після розморозки (0-120 хв)
- **Display during defrost** — "заморожена" температура або "-d-" на Dashboard

Equipment: night_input role + digital input. 80 state keys, 206 pytest тестів.

---

## 📋 Заплановані фази

### Phase 7b-c — Svelte WebUI: Polish
**Пріоритет:** СЕРЕДНІЙ.

- [ ] Графіки температури (DataLogger module + GET /api/log)
- [ ] Animations/transitions
- [ ] Light theme toggle
- [ ] i18n (UA/EN)
- [ ] PWA / Service Worker
- [ ] CMake інтеграція (auto-build Svelte перед idf.py build)

**Оцінка:** 2-3 сесії

### Phase 8 — LCD/OLED Display
**Пріоритет:** СЕРЕДНІЙ — потрібний для standalone контролерів.

- [ ] Display Service (абстракція SSD1306/SH1106/ST7920)
- [ ] display_screens.h вже генерується — підключити до рендеру
- [ ] Навігація: кнопки або енкодер
- [ ] Екрани: Dashboard, Settings, Alarms, System Info

**Оцінка:** 3-4 сесії

### Phase 11b — Multi-sensor + PID
**Пріоритет:** СЕРЕДНІЙ.

- [ ] Кілька DS18B20 на одній шині (address-based binding)
- [ ] NTC сенсор (аналоговий, через ADC)
- [ ] PID регулювання (замість on/off гістерезису)
- [ ] Pressure sensor підтримка

**Оцінка:** 3-4 сесії

### Phase 11c — Advanced Algorithms (Danfoss-inspired)
**Пріоритет:** СЕРЕДНІЙ.

- [ ] Fan cycling (duty cycle коли компресор OFF) — ERC 213
- [ ] Adaptive Defrost (пропуск відтайки за аналізом випарника) — AK-CC 550
- [ ] Condenser overheat protection (alarm при перегріві) — ERC 214
- [ ] Pump Down перед defrost (спорожнення випарника) — AK-CC 550

**Оцінка:** 2-3 сесії

### Phase 12 — mDNS + Cloud
**Пріоритет:** НИЗЬКИЙ.

Вже реалізовано: WiFi STA, AP fallback, SNTP.
- [ ] mDNS (modesp-xxxx.local)
- [ ] MQTT auto-discovery (Home Assistant)
- [ ] Cloud connector (опціонально)

**Оцінка:** 2-3 сесії

### Phase 13 — Modbus + Industrial Protocols
**Пріоритет:** НИЗЬКИЙ (для великих інсталяцій).

- [ ] Modbus RTU (RS485) — slave mode
- [ ] Modbus TCP — slave mode
- [ ] Register map з manifest (auto-generated)

**Оцінка:** 3-4 сесії

---

## Візуальна дорожня карта

```
2026 Q1                    Q2                     Q3
─────────────────────────────────────────────────────────
 ✅ Phase 1-6.5           ✅ Phase 9             📋 Phase 7b-c
 (фундамент, MQTT,        (4 промислові           (графіки, PWA)
  OTA, persist,             модулі)
  Svelte UI)              ✅ Phase 9.5           📋 Phase 8
                           (audit + bugfixes)      (LCD/OLED)
 M1 ✅  M2 ✅             ✅ Phase 10.5
                           (features system)      📋 Phase 11b-c
                          ✅ Phase 11a              (multi-sensor,
                            M3 ✅                   advanced alg.)
─────────────────────────────────────────────────────────
                                                  2027 Q1
                                                  📋 Phase 12-13
                                                   (mDNS, Modbus)
                                                   M4
```

---

## Milestones

| # | Назва | Фази | Статус |
|---|-------|------|--------|
| M1 | Лабораторний прототип | 1-5c | ✅ 2026-02-17 |
| M2 | Connected Controller | 6 | ✅ 2026-02-17 |
| M3 | Production Ready | 9 + 9.5 + 10.5 | ✅ 2026-02-20 |
| M4 | Industrial Grade | 11a + 11b + 11c + 12 + 13 | 📋 Заплановано |

---

## Принципи розвитку

1. **Manifest-first** — нова функціональність починається з manifest schema
2. **Zero heap в hot path** — ETL замість STL, constexpr де можливо
3. **Генератор як bridge** — Python зв'язує manifest з C++ та Web
4. **Документація = частина продукту** — оновлюється з кодом
5. **Тести перед фічами** — тести пишуться разом з кодом

---

## Changelog
- v16.0 (2026-02-21) — Phase 11a DONE: Night Setback, Post-defrost suppression, Display during defrost.
- v15.0 (2026-02-21) — Повне перезаписування: стиснуто завершені фази, видалено дублювання з CLAUDE.md.
- v14.0 (2026-02-20) — Phase 10.5 + 9.5 DONE. M3 ДОСЯГНУТО. 209 tests, 5 artifacts.
- v12.0 (2026-02-18) — Phase 9 COMPLETE: 4 industrial modules.
- v8.0 (2026-02-17) — Phase 6+6.5+7a DONE. M1+M2 ДОСЯГНУТО.
- v3.0 (2026-02-16) — Створено.
