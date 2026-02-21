# ModESP v4 — Архітектурний ревью

> Дата: 2026-02-19
> Ревізія коду: Phase 9.4 (4 модулі, 69 state keys, 79 pytest тестів)

---

## 1. Сильні сторони архітектури

### 1.1 Manifest-First генерація
Один JSON-маніфест на модуль → Python генератор → 4 артефакти (ui.json, state_meta.h, mqtt_topics.h, display_screens.h). Single Source of Truth виключає розсинхронізацію між WebUI, C++ кодом і MQTT топіками. Рідкість для ESP32 проектів — зазвичай UI і firmware розходяться вже на другому місяці.

### 1.2 Zero-Heap дисципліна
ETL замість STL скрізь у hot path. `etl::string<N>`, `etl::vector<T,N>`, `etl::unordered_map` з фіксованою ємністю. Для промислового контролера 24/7 — це критично: ніякої фрагментації heap, передбачувана поведінка, детерміністичний час виконання.

### 1.3 Equipment Manager як єдиний HAL-owner
Бізнес-модулі не мають прямого доступу до HAL — тільки через SharedState requests. Арбітраж (Protection > Defrost > Thermostat) та інтерлоки (тен↔компресор, тен↔клапан ГГ) реалізовані централізовано в одному місці. Модулі можна тестувати без реального заліза.

### 1.4 DAG залежностей без циклів
```
Equipment(0) → publishes sensor facts
Protection(1) → reads facts, publishes alarms
Thermostat(2) → reads facts + alarms, publishes requests
Defrost(2)    → reads facts + alarms, publishes requests
Equipment(0)  → next cycle: arbitrates requests → applies outputs
```
Один цикл затримки між request і apply — прийнятно для 10 Hz loop.

### 1.5 DS18B20 драйвер промислової якості
Bit-bang OneWire з `portDISABLE_INTERRUPTS`, CRC8, фільтр 85°C reset-value, rate-of-change валідація, retry pattern. Для холодильного обладнання де хибне показання = зіпсований товар — правильний підхід.

### 1.6 Defrost 7-фазна state machine
Чистий `enter_phase()` з table-driven relay requests. Delta-update для requests (мінімізує SharedState записи). Skip optimization (випарник чистий → відтайка не потрібна). Consecutive timeout tracking для сервісної діагностики.

### 1.7 Трифазний boot з dependency ordering
PersistService (CRITICAL) стартує до бізнес-модулів — коли Thermostat читає `thermostat.setpoint`, значення вже відновлене з NVS.

---

## 2. Виявлені проблеми

### 2.1 КРИТИЧНІ — впливають на надійність

#### P1: Відсутність C++ юніт-тестів для бізнес-логіки
**Опис:** 79 pytest тестів покривають тільки Python генератор (generate_ui.py). C++ бізнес-логіка (thermostat state machine, protection alarms, defrost 7-phase cycle, equipment arbitration) не має жодного тесту.

**Ризик:** Регресії при будь-якій зміні бізнес-логіки виявляються тільки на залізі. Для промислового контролера це неприйнятно — помилка в defrost може призвести до пошкодження обладнання або зіпсованого товару.

**Рішення:**
- Використати ESP-IDF `linux` target для host-based тестування (без реального ESP32)
- Абстрагувати SharedState за інтерфейсом → mock в тестах
- Мінімальний набір: state machine transitions для кожного модуля, arbitration matrix, interlock verification
- Фреймворк: `unity` (вбудований в ESP-IDF) або `Catch2`
- Оцінка: ~2-3 дні на інфраструктуру + ~1 день на модуль

#### P2: PersistService debounce 5s — ризик втрати критичних параметрів
**Опис:** Якщо живлення зникає протягом 5 секунд після зміни setpoint або alarm limit — зміна втрачається. NVS flush відбувається тільки після debounce.

**Ризик:** Оператор змінює setpoint через WebUI → пропадає живлення → контролер перезавантажується зі старим setpoint → товар зіпсований.

**Рішення:**
- Додати `priority` флаг в state_meta: `persist_priority: "immediate" | "deferred"`
- Критичні параметри (setpoint, alarm limits) → immediate NVS flush (без debounce)
- Некритичні (fan_delay, drip_time) → залишити debounce 5s
- Альтернатива: зменшити debounce до 1s для всіх persist keys (NVS має ~100K циклів запису, при 10 змінах/день = 27 років)

---

### 2.2 ВАЖЛИВІ — впливають на масштабованість

#### P3: SharedState capacity 96 — жорстка стеля
**Опис:** Зараз 69 ключів, ліміт 96. Кожен новий модуль додає 10-20 ключів. Phase 10 (Multi-sensor + PID) + Phase 12 (Modbus) = ще ~30-40 ключів. При переповненні `etl::unordered_map::insert()` мовчки провалюється.

**Ризик:** Після додавання 2-3 нових модулів система мовчки втрачає state keys без будь-якої помилки.

**Рішення:**
- **Короткостроково:** збільшити MODESP_MAX_STATE_ENTRIES до 128 (додаткові ~3KB RAM, прийнятно для ESP32 з 320KB)
- **Середньостроково:** додати runtime перевірку: `if (map_.full()) ESP_LOGE(TAG, "SharedState FULL!")` + buzzer/LED alarm
- **Довгостроково:** namespace-based підхід — окремі sub-maps для кожного модуля, зменшення contention на mutex

#### P4: sync_settings() кожен on_update() — зайве навантаження
**Опис:** Кожен модуль кожен цикл (100ms) перечитує ВСІ свої параметри з SharedState. Defrost: 13 параметрів × 10 Hz = 130 `state_get()` на секунду тільки від одного модуля. Сумарно система робить ~400+ state_get() на секунду тільки для sync_settings().

**Ризик:** Не критичний зараз (SharedState lookup ~O(1) для hash map), але при зростанні кількості модулів mutex contention збільшується.

**Рішення:**
- Варіант А: SharedState version tracking — sync тільки коли `state_.version() != last_synced_version_`
- Варіант Б: Message bus notification — settings_changed message → sync тільки при отриманні
- Варіант В (найпростіший): sync_settings() кожен N-й цикл (раз на секунду замість кожні 100ms)

#### P5: WebSocket — повний state dump при кожній зміні
**Опис:** WsService відправляє ВЕСЬ SharedState при кожній зміні version counter. 69+ ключів = ~2-3KB JSON на frame. З 4 WS клієнтами = 8-12KB/s outbound.

**Ризик:** При зростанні до 100+ ключів і частих змінах (10 Hz sensor updates) — перевантаження WiFi та CPU на JSON серіалізацію.

**Рішення:**
- Delta protocol: відправляти тільки змінені ключі з моменту останнього broadcast
- SharedState.changes_since(version) → повертає тільки нові/змінені пари
- Fallback: повний dump при першому підключенні клієнта або при значному відставанні версій

---

### 2.3 ПОКРАЩЕННЯ — code quality та maintainability

#### P6: Дублювання read_float/read_bool/read_int у 3 модулях
**Опис:** Thermostat, Defrost і Protection мають ідентичний код helper-функцій для читання з SharedState (~15 рядків × 3 модулі = 45 рядків дублювання).

**Рішення:**
```cpp
// В BaseModule або окремому state_helpers.h
class BaseModule {
protected:
    float   state_read_float(const char* key, float def = 0.0f);
    bool    state_read_bool(const char* key, bool def = false);
    int32_t state_read_int(const char* key, int32_t def = 0);
};
```
Один раз реалізувати в BaseModule — всі модулі наслідують автоматично.

#### P7: String comparison для defrost phase detection
**Опис:** Thermostat порівнює рядок з SharedState для визначення FAD фази:
```cpp
if (sp && *sp == "fad") { defrost_fad_ = true; }
```

**Ризик:** Крихке зв'язування — якщо Defrost змінить "fad" на "FAD" або "fan_after_defrost", Thermostat зламається без warning.

**Рішення:**
- Додати explicit bool key `defrost.fad_active` в Defrost manifest
- Defrost встановлює `defrost.fad_active = true` тільки в FAD фазі
- Thermostat читає bool замість string comparison
- Загальне правило: міжмодульна комунікація через bool/float/int, не string enum

#### P8: Відсутність production board config
**Опис:** board.json має 1 GPIO output (relay_1 → GPIO 2), bindings.json прив'язує тільки compressor. Thermostat генерує requests для evap_fan, cond_fan. Equipment обробляє req.heater, req.hg_valve. На реальній платі потрібно 5-7 relay outputs + 2 sensor inputs.

**Рішення:**
- Створити `data/board_production.json` з повним набором GPIO для production PCB
- Створити `data/bindings_production.json` з прив'язками всіх 7 actuators + 2 sensors
- Скрипт/параметр для перемикання: `idf.py build -DBOARD=production`

#### P9: Однопотоковий main loop без isolation
**Опис:** Весь `update_all()` виконується послідовно в одному таску. Якщо один модуль "зависне" в on_update() — зупиняються всі. WatchdogService може виявити, але restart_module() — грубий механізм.

**Рішення:**
- **Короткостроково:** додати per-module timeout в update_all() — якщо модуль не завершив on_update() за N ms, логувати WARNING
- **Довгостроково:** кожен модуль в окремому FreeRTOS таску з власним watchdog (значно збільшує складність, рекомендується тільки для Phase 12+)
- **Прагматично:** залишити як є — для 4 модулів з детерміністичним кодом (без I/O в on_update) це прийнятний trade-off

#### P10: NVS persist ключі — магічні індекси "p0", "p1", ...
**Опис:** PersistService використовує позиційні індекси ("p0", "p1", ...) як NVS ключі на основі порядку в STATE_META масиві. Якщо порядок зміниться (додавання/видалення модуля) — збережені значення присвояться не тим параметрам.

**Ризик:** При оновленні firmware з новим модулем — setpoint може стати alarm_limit, а differential — drip_time.

**Рішення:**
- Варіант А: використовувати hash від state key name як NVS ключ (наприклад, CRC16 від "thermostat.setpoint" → "a3f2")
- Варіант Б: зберігати NVS ключ як скорочений state key ("t.sp", "d.dit", "p.hal")
- Варіант В: NVS migration — при зміні STATE_META зберігати version number, при невідповідності — скинути до defaults

---

## 3. Пріоритезація

| # | Проблема | Критичність | Зусилля | Рекомендація |
|---|----------|-------------|---------|--------------|
| P1 | Немає C++ тестів | 🔴 Критична | 4-5 днів | Phase 10 prerequisite |
| P2 | PersistService debounce | 🔴 Критична | 0.5 дня | Перед production deploy |
| P10 | NVS магічні індекси | 🟠 Важлива | 1 день | Перед production deploy |
| P3 | SharedState capacity | 🟠 Важлива | 0.5 дня | Перед Phase 10 |
| P6 | Дублювання helpers | 🟡 Покращення | 0.5 дня | При нагоді |
| P7 | String comparison | 🟡 Покращення | 0.5 дня | При нагоді |
| P5 | WS full dump | 🟡 Покращення | 1-2 дні | Перед Phase 11 (Cloud) |
| P4 | sync_settings() overhead | 🟢 Низька | 0.5 дня | Після профілювання |
| P8 | Production board config | 🟡 Покращення | 0.5 дня | При наявності PCB |
| P9 | Single-thread loop | 🟢 Низька | 3+ днів | Не рекомендується зараз |

---

## 4. Рекомендований порядок дій для M3 (Production Ready)

1. **P6** — Винести helpers в BaseModule (30 хв, прибирає дублювання перед тестами)
2. **P7** — Замінити string comparison на bool key (30 хв)
3. **P1** — C++ unit tests інфраструктура + тести для 4 модулів (4-5 днів)
4. **P2** — Immediate persist для критичних параметрів (0.5 дня)
5. **P10** — Стабільні NVS ключі замість позиційних індексів (1 день)
6. **P3** — Збільшити SharedState capacity + runtime overflow check (0.5 дня)
7. **P5** — WS delta protocol (1-2 дні, перед Phase 11)

**Загальна оцінка: ~8-9 днів до Production Ready рівня.**

---

## Changelog
- 2026-02-19 — Створено. Повний архітектурний ревью 4 модулів + інфраструктури.
