# ModESP v4 — Changelog

> Повний changelog проекту. Останні зміни також дублюються в CLAUDE.md.

## 2026-03-01

- Phase 12a DONE: KC868-A6 board support. I2C bus + PCF8574 expander підтримка в HAL.
  pcf8574_relay driver (actuator через I2C), pcf8574_input driver (sensor через I2C).
  board_kc868a6.json (6 реле PCF8574 @0x24, 6 входів PCF8574 @0x22).
  100% backward compatible з dev board.
- defrost_relay merger: heater + hg_valve → єдиний defrost_relay role.
  EquipmentModule: defrost_relay_ замість heater_/hg_valve_. Один інтерлок замість двох.
  Defrost: req.defrost_relay замість req.heater/req.hg_valve.
  Equipment manifest: defrost_relay role з driver ["relay", "pcf8574_relay"].
- Heap optimization: NVS batch API (batch_open/batch_close — один handle для flush_to_nvs),
  WS broadcast interval 1000→3000ms, float rounding (roundf 0.01°C),
  thermostat publish debounce (effective_setpoint, display_temp),
  heap guard 40KB (skip WS send), system.heap_largest diagnostics.
- Host C++ unit tests: tests/host/ з doctest (90 test cases для thermostat/defrost/protection).
- Documentation refactoring: all docs audited and updated to match actual code state.
  53 STATE_META, 37 MQTT pub, 52 MQTT sub, 6 drivers, 264 pytest + 90 doctest.

## 2026-02-24

- Phase 14b DONE: 6-channel dynamic DataLogger + ChartWidget. TempRecord 12→16 bytes (ch[6] array),
  ChannelDef compile-time table (air/evap/cond/setpoint/humidity/reserved), sync/sample/serialize loops.
  Manifest: +log_setpoint, +log_humidity, "Канали логування" card. ChartWidget: fully dynamic channels
  from API, PALETTE colors, toggle checkboxes, setpoint dual-mode. JSON v3 with dynamic channels.
  i18n: +chart.ch_setpoint, +chart.ch_humidity. 97 state keys, 48 STATE_META, 46 MQTT sub, 264 тестів.
- Phase 7b-c DONE: WebUI Polish. Light/Dark theme toggle (stores/theme.js, CSS custom properties,
  localStorage persist, prefers-color-scheme). i18n UA/EN (stores/i18n.js, ~75 keys × 2 languages, derived $t store).
  Animations: page fly/fade transitions, staggered card entrance, card slide collapse, value flash on change.
  19 files updated. Bundle: 37.5→43.7KB gz (within 50KB limit).
- Phase 14a: Multi-channel DataLogger (air+evap+cond), TempRecord 8→12 bytes,
  TEMP_NO_DATA sentinel, JSON v2 з channels header, auto-migration old format.
  ChartWidget: multi-line chart (3 polylines), channel toggles, event text list (50 events),
  CSV export (client-side). Equipment: +has_cond_temp. Generator: +cond_temp FEATURE_TO_STATE.
  Fix: Cache-Control no-store (was max-age=86400 causing stale bundle).
  95 state keys, 46 STATE_META, 37 MQTT pub, 44 MQTT sub, 207 тестів.
- Phase 14 DONE: DataLogger module (append+rotate LittleFS, streaming chunked JSON,
  10 event types, 6 state keys) + ChartWidget (SVG polyline, min/max downsample, comp/defrost zones,
  tooltip, 24h/48h toggle). GET /api/log, GET /api/log/summary. downsample.js utility.
  Tech debt: TIMER_SATISFIED, Cache-Control, AUDIT-012 separate alarm delays, AUDIT-036 CLOSED.
  92 state keys, 44 STATE_META, 37 MQTT pub, 42 MQTT sub, 207 тестів. 5 modules, 9 pages.

## 2026-02-23

- Phase 13a DONE: Runtime UI visibility (visible_when + requires_state). Manifests: constraints
  з disabled_hint, visible_when на defrost/thermostat/protection cards/widgets. Generator: resolve_constraints()
  зберігає ВСІ options + requires_state (FEATURE_TO_STATE mapping), visible_when passthrough, V19 validation.
  Svelte: isVisible() utility, SelectWidget per-option disabled, DynamicPage visible_when. Equipment: +3 has_* keys
  (has_cond_fan, has_door_contact, has_evap_temp). 84 state keys, 178 тестів. Runtime: Bindings→Save→Restart→enabled.
- Phase 11b COMPLETE: SEARCH_ROM (Maxim AN187 binary search), GET /api/onewire/scan endpoint
  (scan bus → devices with temperature + assigned status), WebUI OneWire Discovery in BindingsEditor
  (scan button, device list, role assignment). HttpService: set_hal() injection for scan.

## 2026-02-22

- Phase 11b DONE: Multi DS18B20 (MATCH_ROM + CRC8 validation), NTC/ADC driver (B-parameter),
  DigitalInput C++ driver (50ms debounce). HAL: Binding.address, GpioInputConfig, AdcChannelConfig.
  config_service: parse address/gpio_inputs/adc_channels. DriverManager: digital_input + ntc pools.
  Equipment: condenser_temp (NTC/DS18B20) + door_contact (DigitalInput). 5 drivers.
  81 state keys (was 80), 39 STATE_META, 34 MQTT pub, 38 MQTT sub. 206 tests green.

## 2026-02-21

- Phase 11a DONE: Night Setback (4 modes, SNTP schedule, DI, manual),
  Post-defrost alarm suppression (0-120 min timer), Display during defrost (real/frozen/-d-).
  Equipment: night_input role + digital input binding. Thermostat: effective_setpoint, display_temp,
  is_night_active(). Protection: post_defrost_delay, suppress_high flag. Dashboard: display_temp + NIGHT badge.
  80 state keys (was 70), 39 STATE_META, 33 MQTT pub, 38 MQTT sub, 13 menu items. 206 tests green.

## 2026-02-20

- Phase 10.5 DONE: Features System + Select Widgets. Manifests: features/constraints/options
  in thermostat/defrost/protection. Generator: FeatureResolver, select widgets, disabled+reason,
  FeaturesConfigGenerator → features_config.h (5th artifact). C++: has_feature() in BaseModule,
  guards in thermostat/defrost/protection. digital_input driver manifest. board.json: 4 relays, 1 DI, 2 ADC.
  Validation V14-V18. 209 pytest tests green (43 new in test_features.py + 4 binding fixtures).
- BUG-012: NVS positional keys → hash-based (djb2). Auto-migration p0..p32 → sXXXXXXX.
  BUG-023: POST /api/settings uses meta->type instead of decimal point heuristic. Float persist fixed.
  AUDIT-014..017: manifest range fixes. AUDIT-038..040: security (CORS, traversal, old files removed).
- AUDIT Phase 10: 10 critical fixes. C++: relay min_switch_ms role-based (compressor only),
  EM publishes actual relay state via get_state(), EM-level compressor anti-short-cycle timer
  (COMP_MIN_OFF_MS=180s, COMP_MIN_ON_MS=120s), JSON string escaping in http_service + ws_service.
  WebUI: ButtonWidget state key fallback (manual defrost works), icons (flame, shield-alert,
  alert-triangle, thermometer), Dashboard uses equipment.compressor + defrost/alarm tiles,
  StatusText defrost phase colors, alarm banner in Layout, apiPost error handling.

## 2026-02-18

- SharedState capacity 64→96 (MODESP_MAX_STATE_ENTRIES). 69 manifest keys + ~15 system keys overflowed 64.
- Phase 9.4 DONE: Defrost module (modules/defrost/). 7-phase state machine,
  3 types (natural/heater/hot gas), 4 initiations (timer/demand/combo/manual).
  13 persist params + 2 runtime persist (interval_timer, defrost_count). 27 state keys, 10 MQTT publish.
  Generator fix: read-only persist keys now included in state_meta.h (writable=false, persist=true).
  4 modules, 69 state keys, 9 pages. 79 тестів зелені.
- Phase 9.3 DONE: Protection module (modules/protection/). 5 alarm monitors
  (HAL, LAL, ERR1, ERR2, Door). Delayed alarms (dAd), defrost blocking, auto-clear + manual reset.
  5 persist параметрів, 14 state keys, 8 MQTT publish. 79 тестів зелені. 3 modules, 42 state keys.
- Phase 9.2 DONE: Thermostat v2 — повна логіка spec_v3. Асиметричний диференціал,
  state machine (STARTUP→IDLE→COOLING→SAFETY_RUN), вент. випарника (3 режими FAn), вент. конденсатора
  (затримка COd), Safety Run, 11 persist параметрів, 18 state keys. 79 тестів зелені.
- Phase 9.1 DONE: Equipment Manager (modules/equipment/). Єдиний власник HAL drivers.
  Арбітраж: Protection > Defrost > Thermostat. Інтерлоки: тен↔компресор, тен↔клапан ГГ.
  Thermostat рефакторинг: req.compressor замість direct relay, читає equipment.air_temp.
  generate_ui.py: cross-module widget key resolution (inputs → global state map). 79 тестів зелені.

## 2026-02-17

- Phase 7a DONE: Svelte WebUI (webui/). Svelte 4 + Rollup. 14 widget components,
  Dashboard (tile-based, temp color zones, compressor pulse), Layout (sidebar + bottom tabs),
  DynamicPage (renders any ui.json page). Bundle: 17KB gzipped. Deploy: npm run deploy → data/www/.
- Phase 6.5 DONE: PersistService (CRITICAL, Phase 1). SharedState persist callback (ПОЗА mutex).
  state_meta.h: persist+default_val. POST /api/settings: state_meta валідація (writable, min/max clamp).
  Thermostat: hardcoded config.setpoint замінено на SharedState read. 79 pytest тестів зелені.
- Inputs validation в generate_ui.py: _validate_inputs() в ManifestValidator, 6 правил з docs/10 §3.2a.
  73 pytest тести зелені. Thermostat без inputs (єдиний модуль) — працює як раніше.
- Phase 6 DONE: MQTT WebUI page додана (generate_ui.py + app.js). mqtt.broker в SharedState.
  WiFi PS bug виправлений (DS18B20 esp_wifi_set_ps видалений). Thermostat: temperature→SharedState,
  settings sync, gauge→value. OTA + MQTT endpoints додані в HTTP API таблицю. Milestone M2 ДОСЯГНУТО.
- Driver manifests (ds18b20, relay) + DriverManifestValidator + cross-валідація module↔driver.
  board.json: relays→gpio_outputs. C++ HAL оновлений (BoardConfig.gpio_outputs). Bindings page в WebUI.
  generate_ui.py: ~900 рядків, --drivers-dir, 66 тестів зелені.
- Видалено HTMLGenerator з generate_ui.py (820→755 рядків, 4 артефакти замість 5). WebUI тепер статичний (data/www/)
- Додано правила документування. WiFi: виправлено (STA працює, не тільки AP)

## 2026-02-16

- Створено
