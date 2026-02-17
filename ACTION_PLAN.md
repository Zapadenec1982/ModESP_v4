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
- ✅ 79 pytest тестів для генератора (всі зелені, включаючи inputs validation + persist)
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

### Що НЕ працює:
- ❌ MQTT auto-discovery (Home Assistant) — не реалізовано
- ❌ MQTT publish state при зміні — потрібно верифікувати
- ❌ Alarm/Defrost модулі
- ❌ WebUI примітивний (vanilla JS, ~30KB)
- ❌ LCD/OLED
- ❌ mDNS (hostname.local)

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

### Архітектурні рішення (прийняті 2026-02-17)

| Тема | Рішення | Обґрунтування |
|------|---------|---------------|
| **Module arbitration** | Convention через inputs (SharedState ключі). Модулі самі вирішують на основі domain logic | Взаємовиключні модулі (thermostat↔defrost) блокують ОДИН ОДНОГО, не relay. Enforcement на рівні HAL — передчасна оптимізація |
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

### КРОК 4: Svelte WebUI (Phase 7)

```
  1. [ ] Svelte проект (webui/), rollup, build pipeline
  2. [ ] WebSocket store (reactive state)
  3. [ ] Tile-based dashboard (CSS Grid, color zones з operational_range)
  4. [ ] Widgets: GaugeTile, SliderTile, ToggleTile, ChartTile
  5. [ ] System pages: WiFi, OTA, Module Status, About
  6. [ ] Driver Settings page (service access level)
  7. [x] Спростити generate_ui.py → HTMLGenerator видалений, WebUI статичний
```

---

### КРОК 5: Промислові модулі (Phase 9)

```
  1. [ ] Alarm Module (high/low temp alerts, HACCP logging)
  2. [ ] Defrost Module (timer + temp termination, drip, fan delay)
  3. [ ] Door Module (digital input, delay alarm)
  Кожен модуль: manifest.json → генерація → C++ код → тести
```

**Milestone M3: "Production Ready" — після Phase 7 + 9.**

---

### КРОК 6+ (менш терміново)

- **Phase 8:** LCD/OLED display (display_screens.h вже генерується)
- **Phase 10:** Multi-sensor + PID
- **Phase 11:** NTP + mDNS (WiFi STA вже працює!)
- **Phase 12:** Modbus RTU/TCP

---

## Правило одного фокусу

**Працюй тільки над ОДНИМ кроком одночасно.**

```
  [✅ done]       [✅ done]      [✅ done]        [◀── ТУТ]       [далі]
Стабілізація  →  MQTT/OTA  → Auto-persist  →  Svelte UI  → Промислові модулі
     ↓              ↓             ↓               ↓                ↓
  "працює       "можна       "налаштування    "красиво"       "можна
   надійно"      розгорнути"  зберігаються"                    продавати"
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
