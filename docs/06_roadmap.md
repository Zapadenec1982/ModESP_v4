# ModESP v4 — Дорожня карта розвитку

> Останнє оновлення: 2026-02-23
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

### Phase 11b — Multi DS18B20 + NTC/ADC + DigitalInput (завершено 2026-02-22)

Розширення HAL для підтримки кількох сенсорів та нових типів драйверів:
- **DS18B20 MATCH_ROM** — адресне читання конкретного датчика на шині (64-bit ROM address)
- **NTC driver** — аналоговий термістор через ADC1 (B-parameter equation, 12-bit)
- **DigitalInput driver** — GPIO input з 50ms debounce (door_contact, night_input)
- **HAL types** — Binding.address, GpioInputConfig, AdcChannelConfig, ресурси
- **config_service** — парсинг address, gpio_inputs, adc_channels з board.json/bindings.json
- **Equipment** — condenser_temp (NTC/DS18B20) + door_contact (DigitalInput)

5 драйверів (DS18B20, Relay, DigitalInput, NTC). 81 state keys, 206 pytest тестів.

### Phase 13a — Runtime UI Visibility (завершено 2026-02-23)

Повністю runtime-механізм для керування видимістю і доступністю UI елементів:
- **visible_when** — cards та widgets ховаються/показуються на основі state key значень (eq/neq/in)
- **requires_state (per-option disabled)** — select options завжди в ui.json, disabled якщо hardware відсутнє
- **FEATURE_TO_STATE mapping** — feature name → equipment.has_* state key (6 mappings)
- **Equipment has_* keys** — has_heater, has_hg_valve, has_cond_fan, has_door_contact, has_evap_temp
- **Svelte** — isVisible() utility, SelectWidget per-option disabled з hint, DynamicPage visible_when
- **Runtime ланцюг** — Bindings page → Save → Restart → equipment.has_* = true → option/card enabled

84 state keys, 178 pytest тестів.

### Phase 14 + 14a — DataLogger + Multi-channel (завершено 2026-02-24)

Повноцінний логер температури та подій з LittleFS:
- **Multi-channel logging** — air (завжди) + evap + cond (per-channel toggle, visible_when)
- **TempRecord 12 bytes** — 3 канали + reserved, TEMP_NO_DATA sentinel (INT16_MIN)
- **Append-only + rotate** — temp.bin/events.bin з auto-rotate при перевищенні retention
- **10 event types** — compressor, defrost, alarm, door, power_on (edge-detect)
- **Streaming chunked JSON v2** — GET /api/log?hours=24, без великого буфера в RAM
- **ChartWidget** — multi-line SVG chart, channel toggles, comp/defrost zones, tooltip
- **Event list** — текстові мітки подій під графіком (останні 50)
- **CSV export** — client-side download (temps + events), zero ESP32 навантаження
- **Auto-migration** — старий 8-byte формат видаляється при boot

95 state keys, 207 pytest тестів. Equipment: +has_cond_temp.

---

## 📋 Заплановані фази

### Phase 7b-c — Svelte WebUI: Polish
**Пріоритет:** СЕРЕДНІЙ.

- [x] ~~Графіки температури (DataLogger module + GET /api/log)~~ (Phase 14+14a)
- [x] ~~Animations/transitions~~ (Phase 7b-c)
- [x] ~~Light theme toggle~~ (Phase 7b-c)
- [x] ~~i18n (UA/EN)~~ (Phase 7b-c)
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

### Phase 11c — PID + Advanced Sensors
**Пріоритет:** СЕРЕДНІЙ.

- [ ] PID регулювання (замість on/off гістерезису)
- [ ] Pressure sensor підтримка
- [x] ~~SEARCH_ROM — auto-learn DS18B20 через WebUI (scan → assign → save)~~ (Phase 11b)

**Оцінка:** 2-3 сесії

### Phase 11d — Advanced Algorithms (Danfoss-inspired)
**Пріоритет:** СЕРЕДНІЙ.

- [ ] Fan cycling (duty cycle коли компресор OFF) — ERC 213
- [ ] Adaptive Defrost (пропуск відтайки за аналізом випарника) — AK-CC 550
- [ ] Condenser overheat protection (alarm при перегріві) — ERC 214
- [ ] Pump Down перед defrost (спорожнення випарника) — AK-CC 550

**Оцінка:** 2-3 сесії

### Phase 12a — KC868-A6 Board Support (I2C Expander)
**Пріоритет:** ВИСОКИЙ.

- [ ] I2C bus + PCF8574 expander підтримка в HAL
- [ ] PCF8574RelayDriver + PCF8574InputDriver
- [ ] board_kc868a6.json + bindings_kc868a6.json
- [ ] 100% backward compatible з dev board

**Оцінка:** 1-2 сесії. Промпт: `prompt_phase12a.md`

### Phase 12b — mDNS + Cloud
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
                           (features system)      📋 Phase 11c-d
                          ✅ Phase 11a              (PID, advanced)
                          ✅ Phase 11b
                            (multi-sensor)
                          ✅ Phase 13a            📋 Phase 12a
                            (runtime UI)            (KC868-A6)
                            M3 ✅
─────────────────────────────────────────────────────────
                                                  2027 Q1
                                                  📋 Phase 12b-13
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
| M4 | Industrial Grade | 11a-d + 12a-b + 13 | 📋 Заплановано |

---

## Принципи розвитку

1. **Manifest-first** — нова функціональність починається з manifest schema
2. **Zero heap в hot path** — ETL замість STL, constexpr де можливо
3. **Генератор як bridge** — Python зв'язує manifest з C++ та Web
4. **Документація = частина продукту** — оновлюється з кодом
5. **Тести перед фічами** — тести пишуться разом з кодом

---

## Changelog
- v21.0 (2026-02-24) — Phase 7b-c partial: Light/dark theme toggle, i18n UA/EN (~75 keys), Svelte transitions (page fly, card slide, value flash). Bundle 43.7KB gz.
- v20.0 (2026-02-24) — Phase 14+14a DONE: DataLogger + multi-channel (air+evap+cond), event list, CSV export. 95 state keys, 207 tests.
- v18.0 (2026-02-23) — Phase 13a DONE: Runtime UI visibility (visible_when + requires_state). 84 state keys, 178 tests.
- v17.0 (2026-02-22) — Phase 11b DONE: Multi DS18B20 (MATCH_ROM), NTC/ADC driver, DigitalInput C++ driver. 5 drivers, 81 state keys, 206 tests.
- v16.0 (2026-02-21) — Phase 11a DONE: Night Setback, Post-defrost suppression, Display during defrost.
- v15.0 (2026-02-21) — Повне перезаписування: стиснуто завершені фази, видалено дублювання з CLAUDE.md.
- v14.0 (2026-02-20) — Phase 10.5 + 9.5 DONE. M3 ДОСЯГНУТО. 209 tests, 5 artifacts.
- v12.0 (2026-02-18) — Phase 9 COMPLETE: 4 industrial modules.
- v8.0 (2026-02-17) — Phase 6+6.5+7a DONE. M1+M2 ДОСЯГНУТО.
- v3.0 (2026-02-16) — Створено.
