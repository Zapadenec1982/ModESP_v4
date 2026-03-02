# ModESP v4 — План дій

> Точка входу в проект. Відкривай на початку кожної робочої сесії.

---

## Де ми зараз

**Milestone M3: "Production Ready" — ДОСЯГНУТО (2026-02-20).**

Повністю працюючий контролер холодильної камери:
- 5 бізнес-модулів (Equipment Manager, Thermostat v2, Defrost 7-phase, Protection 5 alarms, DataLogger)
- 6 драйверів (DS18B20, Relay, Digital Input, NTC, PCF8574 Relay, PCF8574 Input)
- Features System — UI показує тільки підключене обладнання
- Runtime UI visibility — visible_when + per-option disabled
- Svelte WebUI (21 widget type, Dashboard, dark/light theme, i18n UA/EN, 52KB gzipped)
- MQTT TLS, OTA з rollback, auto-persist NVS
- 264 pytest + 90 host C++ (doctest) тестів
- KC868-A6 board support (Phase 12a)
- 97 state keys, 53 STATE_META, 37 MQTT pub, 52 MQTT sub

**Стабільність:** 30+ годин на реальному KC868-A6. Всі алгоритми працюють.

**Відомі проблеми:**
- Витік пам'яті виправлено костилями (WS heap guard 40KB, broadcast 3000ms)
- WebUI — прототипний рівень, потребує UX/дизайн переосмислення
- Немає HTTP авторизації
- OTA без валідації розміру/підпису

---

## Roadmap: MVP → Production

### ЧАСТИНА 1: MVP (14 сесій)

> **Ціль:** Пристрій для реальних об'єктів під наглядом інженера.
> **Фокус:** WebUI UX + Memory proper fix + базова безпека

| Sprint | Сесії | Тема | Статус |
|--------|-------|------|--------|
| **Sprint 1** | 1-4 | Design System + Memory Fix | |
| **Sprint 2** | 5-8 | Dashboard Redesign + Settings UX | |
| **Sprint 3** | 9-11 | Mobile UX + Security | |
| **Sprint 4** | 12-14 | Finishing Touches + HW Test | |

#### Sprint 1: Design System + Memory Fix (сесії 1-4)

- [x] **Сесія 1a:** Design Tokens + CSS Architecture ✓
  - `webui/src/styles/tokens.css` — spacing, typography, radius, semantic colors, shadows, transitions
  - Import в main.js, MIGRATION.md guide
  - Bundle: 46.4KB JS gz + 7.7KB CSS gz

- [x] **Сесія 1b:** Base Components Refactor ✓
  - Card: variant prop (default/status/alarm), sessionStorage collapse, tokens
  - Toast: bottom-center, close button, max 3, error 8s/warn 5s
  - Layout: connection overlay через 5с disconnect, spinner + retry, toast on reconnect
  - WidgetRenderer: min-height 44px, var(--sp-3) gap
  - i18n: +3 ключі (conn.lost, conn.retry, conn.restored)
  - Bundle: 47.1KB JS gz + 8.1KB CSS gz

- [x] **Сесія 1.1a:** Delta-Only WS Broadcasts ✓
  - SharedState changed_keys_ вектор + track_change параметр
  - WsService: for_each_changed_and_clear() delta, send_full_state_to(fd)
  - BaseModule: state_set(track_change=false) для таймерів/діагностики
  - Fix DataLogger: прибрано NTP guard що блокував on_update()
  - Fix DS18B20: MATCH_ROM only, прибрано auto-scan/auto-assign
  - Fix Bindings: unbind/rebind UI, save без required roles (з confirm)
  - Bundle: 48.0KB JS gz + 8.1KB CSS gz

- [x] **Сесія 1.1b:** Broadcast Tuning + Cleanup ✓
  - Heap guard: 40KB → 16KB/8KB, interval: 3000ms → 1500ms
  - BUG-025: вже вирішений через track_change=false (Sprint 1.1a)

#### Sprint 2: Dashboard Redesign + Settings UX (сесії 5-8)

- [x] **Сесія 1.2a:** Dashboard Layout — СКІП (pills відхилено, dashboard залишається як є)

- [x] **Сесія 1.2b:** Slider + MiniChart Improvements ✓
  - Chart live update ($: reactive), Clock jank fix, WS reconnect fix

- [x] **Сесія 1.3a:** Debounce + Form Behavior ✓
  - createSettingSender() utility, debounce всіх widgets (NumberInput 500ms, Toggle/Select instant)
  - Pending dot, flash border, opacity states

- [x] **Сесія 1.3b:** Form Fixes + Error Handling ✓
  - MqttSave stores, SelectWidget polish, API error enrichment, i18n

#### Sprint 3: Mobile UX + Security (сесії 9-11)

- [ ] **Сесія 1.4:** Mobile UX Polish
  - Bottom tabs max 5 + "More", tablet 1-column, 44px touch targets
  - Промпт: `prompts/sprint3_session1_4_mobile_ux.md`

- [ ] **Сесія 1.5:** HTTP Basic Auth
  - check_auth() middleware, NVS credentials, WebUI login prompt
  - Промпт: `prompts/sprint3_session1_5_http_auth.md`

- [ ] **Сесія 1.6:** OTA Safety
  - Size validation, magic byte, rollback timeout 60s
  - Промпт: `prompts/sprint3_session1_6_ota_safety.md`

#### Sprint 4: Finishing Touches (сесії 12-14)

- [ ] **Сесія 1.7:** MQTT Backoff + WiFi Recovery
  - Exponential backoff, STA watchdog 10 хв
  - Промпт: `prompts/sprint4_session1_7_mqtt_wifi.md`

- [ ] **Сесія 1.8:** °C/°F + Alarm Relay
  - system.temp_unit, format layer, alarm_relay role
  - Промпт: `prompts/sprint4_session1_8_units_alarm.md`

- [ ] **Сесія 1.9:** Hardware Integration Test
  - Повний чеклист на KC868-A6: mobile, OTA, auth, memory, MQTT
  - Промпт: `prompts/sprint4_session1_9_hw_test.md`

---

### ЧАСТИНА 2: PRODUCTION (15-20 сесій)

> **Ціль:** Масове розгортання 100+ пристроїв

| Sprint | Сесії | Тема | Статус |
|--------|-------|------|--------|
| **Sprint 5** | 15-17 | Security Hardening | |
| **Sprint 6** | 18-20 | LCD/OLED Display | |
| **Sprint 7** | 21-22 | Extended Testing | |
| **Sprint 8** | 23 | CI/CD Pipeline | |
| **Sprint 9** | 24-29 | Advanced Features | |
| **Sprint 10** | 30-32 | Production WebUI | |
| **Sprint 11** | 33+ | Fleet Management | |

#### Sprint 5: Security Hardening (3 сесії)
- [ ] HTTPS з self-signed cert per device
- [ ] Secure Boot v2 + Flash Encryption
- [ ] OTA Firmware Signing + CSRF + rate limiting
- Промпти: `prompts/prod_sprint5_security.md`

#### Sprint 6: LCD/OLED Display (3 сесії)
- [ ] SSD1306 I2C driver + display service
- [ ] Screen renderer (display_screens.h) + button navigation
- [ ] Menu system для параметрів без WiFi
- Промпти: `prompts/prod_sprint6_display.md`

#### Sprint 7: Extended Testing (2 сесії)
- [ ] Equipment + DataLogger host tests
- [ ] Driver unit tests + WebUI integration tests (Playwright)
- Промпти: `prompts/prod_sprint7_testing.md`

#### Sprint 8: CI/CD (1 сесія)
- [ ] GitHub Actions: build + pytest + host tests + WebUI build + bundle size check
- Промпт: `prompts/prod_sprint8_cicd.md`

#### Sprint 9: Advanced Features (5-6 сесій)
- [ ] Modbus RTU/TCP
- [ ] Home Assistant MQTT Auto-Discovery + mDNS
- [ ] Scheduled defrost (AUDIT-032)
- [ ] Pressure switches (AUDIT-031)
- [ ] PID controller
- Промпти: `prompts/prod_sprint9_features.md`

#### Sprint 10: Production WebUI (3 сесії)
- [ ] PWA + Service Worker
- [ ] WCAG 2.1 AA accessibility
- [ ] Performance optimization
- Промпти: `prompts/prod_sprint10_webui.md`

#### Sprint 11: Fleet Management (5+ сесій)
- [ ] Backend dashboard (MQTT → Web)
- [ ] Групове OTA
- [ ] Alarm aggregation + notifications
- Промпти: `prompts/prod_sprint11_fleet.md`

---

## Відкриті проблеми

### Memory Workarounds — ВИРІШЕНО (Sprint 1.1a + 1.1b)
| Було | Стало | Статус |
|------|-------|--------|
| WS heap guard 40KB | 16KB (full state) / 8KB (delta/ping) | ✅ Sprint 1.1b |
| Broadcast 3000ms | 1500ms (delta ~200B) | ✅ Sprint 1.1b |
| MQTT timer exclusion (BUG-025) | track_change=false в SharedState | ✅ Sprint 1.1a |
| Float rounding 0.01°C | proper optimization, залишити | ✅ OK |

### Open AUDIT Items (MVP + Production)
| ID | Опис | Sprint |
|----|------|--------|
| AUDIT-030 | HTTP Basic Auth | MVP Sprint 3 (1.5) |
| AUDIT-031 | HP/LP pressure switches | Prod Sprint 9 |
| AUDIT-032 | Scheduled defrost | Prod Sprint 9 |
| AUDIT-033 | Alarm relay output | MVP Sprint 4 (1.8) |
| AUDIT-034 | Password protection | Prod (deferred) |
| AUDIT-035 | °C/°F units | MVP Sprint 4 (1.8) |

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
| **Design System** | Industrial HMI: tokens.css, semantic colors, 44px touch, mobile-first |
| **WS Broadcast** | ✅ Delta-only (Sprint 1.1a): changed_keys_ вектор, ~200B vs 3.5KB, track_change=false для таймерів |

---

## Як працювати з Claude Code

1. Відкрий промпт з `prompts/` для поточної сесії
2. Вставь його як initial prompt для Claude Code
3. Claude Code працює, ти перевіряєш
4. Після сесії — Claude Code оновлює цей файл + CLAUDE.md

**Якщо загубився:**
- `ACTION_PLAN.md` → що робити
- `CLAUDE.md` → як працює проект
- `prompts/` → готові промпти для кожної сесії
- `docs/06_roadmap.md` → куди йдемо

---

## Завершені фази

| Фаза | Що | Коли |
|------|----|------|
| 1-5c | Ядро, HAL, WiFi, HTTP, WS, UI generation, тести | 02-12..02-17 |
| 6 | MQTT TLS + OTA з rollback. **M2 ДОСЯГНУТО** | 02-17 |
| 6.5 | Auto-persist settings (NVS, PersistService) | 02-17 |
| 7a | Svelte WebUI (15 widgets, Dashboard, 17KB gz) | 02-17 |
| 9 | Equipment Manager + Thermostat v2 + Defrost + Protection | 02-18 |
| 9.5 | Audit (3 агенти) + Bugfixes (14/27 fixed) | 02-19..02-20 |
| 10.5 | Features System + Select Widgets (209 тестів). **M3 ДОСЯГНУТО** | 02-20 |
| 11a | Danfoss-level: Night Setback, Post-defrost, Display | 02-21 |
| 11b | Multi DS18B20 (MATCH_ROM) + NTC/ADC + DigitalInput | 02-22 |
| 13a | Runtime UI visibility (visible_when + disabled options) | 02-23 |
| 14+14a+14b | 6-channel DataLogger + ChartWidget | 02-24 |
| 7b-c | WebUI Polish: theme, i18n, animations | 02-24 |
| 12a | KC868-A6: I2C PCF8574 relay + input drivers | 03-01 |

**Детальна історія:** docs/CHANGELOG.md

---

## Changelog

- 2026-03-02 — Session 1.1a DONE: Delta WS broadcasts, DataLogger NTP fix, DS18B20 MATCH_ROM only, Bindings unbind/rebind UI.
- 2026-03-01 — Повний план MVP→Production (14+20 сесій). WebUI UX redesign як #1 пріоритет. Створено prompts/ директорію з промптами для кожної сесії.
- 2026-03-01 — Phase 12a DONE: KC868-A6 board support. Heap optimization.
- 2026-02-24 — Phase 14b + 7b-c DONE: 6-channel DataLogger, WebUI Polish.
- 2026-02-23 — Phase 13a DONE: Runtime UI visibility.
- 2026-02-22 — Phase 11b DONE: Multi DS18B20, NTC/ADC, DigitalInput.
- 2026-02-21 — Phase 11a DONE: Night Setback, Post-defrost, Display.
- 2026-02-20 — Phase 10.5 DONE: Features System. M3 ДОСЯГНУТО.
