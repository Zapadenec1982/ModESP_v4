# ModESP v4 — Верифікація багів (BUGFIXES.md)

**Дата верифікації:** 2026-02-20
**Базовий документ:** `docs/BUGFIXES.md` (2026-02-19)
**Метод:** Порядковий аналіз кожного бага по живому коду з D:\ModESP_v4\

---

## Результат

- **24/24 баги з BUGFIXES.md — ПІДТВЕРДЖЕНІ** (BUG-003 з рекомендацією знизити severity)
- **3 НОВІ баги виявлені** під час верифікації (NEW-001, NEW-002, NEW-003)
- NEW-002 — **найкритичніший баг у проекті** (persist keys мовчки не зберігаються)

---

## SEV-1 — CRITICAL (4 баги)

### BUG-001: StateKey truncation (etl::string<24>)
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `types.h:34` → `MODESP_MAX_KEY_LENGTH = 24`
- `types.h:49` → `using StateKey = etl::string<MODESP_MAX_KEY_LENGTH>`
- ETL `string<24>` вміщує рівно 24 символи (буфер 25 байт)
- Ключі >24 символів МОВЧКИ обрізаються:

| Ключ | Довжина | Статус |
|------|---------|--------|
| `thermostat.req.compressor` | 26 | 🔴 Обрізано |
| `thermostat.cond_fan_delay` | 25 | 🔴 Обрізано |
| `thermostat.safety_run_off` | 25 | 🔴 Обрізано |
| `thermostat.safety_run_on` | 24 | ✅ Вміщується |
| `protection.high_temp_alarm` | 26 | 🔴 Обрізано |
| `protection.low_temp_alarm` | 25 | 🔴 Обрізано |
| `protection.sensor1_alarm` | 24 | ✅ Вміщується |
| `defrost.consecutive_timeouts` | 28 | 🔴 Обрізано |
| `defrost.last_termination` | 24 | ✅ Вміщується |

**⚠️ Уточнення:** BUGFIXES.md стверджує "23 usable chars" — це НЕТОЧНО. ETL `string<24>` = 24 символи. Але реальна проблема з ключами >24 символів — підтверджена.

**Наслідки:** Обрізані ключі створюють колізії. Наприклад, `thermostat.safety_run_on` (24) і `thermostat.safety_run_of` (обрізано з 25) — різні ключі, persist/read працює неправильно.

---

### BUG-002: Defrost type change mid-cycle
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `defrost_module.cpp:56` → `sync_settings()` кожен цикл оновлює `defrost_type_`
- `defrost_module.cpp:413` → `finish_active_phase()` читає поточний `defrost_type_`

```cpp
// finish_active_phase() — рядок 413
if (defrost_type_ == 2) {
    enter_phase(Phase::EQUALIZE);  // Тільки для ГГ
} else {
    enter_phase(Phase::DRIP);
}
```

Якщо тип змінити з 2→0 під час ACTIVE — фаза EQUALIZE пропускається (не вирівнюється тиск → удар ГГ). Протилежна ситуація (0→2) вставить EQUALIZE при природній відтайці (не критично, але неправильно). **SEV-1 підтверджена.**

---

### BUG-003: protection.lockout завжди false
**Вердикт: ⚠️ BY DESIGN (поточна фаза)**

- `protection_module.cpp:12` → коментар: `"protection.lockout = false (reserved for Phase 10+)"`
- `protection_module.cpp:278` → `state_set("protection.lockout", false);` кожен цикл
- Арбітраж Equipment Manager (`equipment_module.cpp:189-192`) перевіряє `protection.lockout` — завжди false

**Факт:** Баг описаний правильно — lockout дійсно завжди false. Але це **задокументоване** як Phase 10+ функція. SEV-1 — лише коли Protection Phase 10 буде реалізована (поки EM арбітраж по lockout мертвий код). **Рекомендую знизити до SEV-2 для поточної фази.**

---

### BUG-004: Condenser fan delay lost during defrost
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `thermostat_module.cpp:231-236`:
```cpp
if (defrost_active_ && !defrost_fad_) {
    request_compressor(false);
    request_evap_fan(false);
    request_cond_fan(false);  // <-- Без затримки COd
    return;                   // <-- update_cond_fan() НІКОЛИ не виконується
}
```
- `thermostat_module.cpp:401-406` → `update_cond_fan()` теж має `if (defrost_active_ && !defrost_fad_) { ... return; }` з `cond_fan_delay_active_ = false;`

Результат: при старті defrost конд. вентилятор вимикається миттєво (замість COd затримки), що теоретично може пошкодити компресор (тиск конденсатора). **SEV-1 підтверджена.**

---

## SEV-2 — MAJOR (10 багів)

### BUG-005: Thermostat state not reset during defrost
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `thermostat_module.cpp:231` → при `defrost_active_`, `request_compressor(false)` встановлює `compressor_on_ = false`
- Але `state_` **НЕ змінюється** — залишається COOLING
- `comp_off_time_ms_` починає рахувати (бо `compressor_on_ == false`), але state_ = COOLING
- Після defrost (повернення до state machine) — `state_ == COOLING` з `comp_off_time_ms_` вже великим
- При `current_temp_ <= setpoint_` && `comp_on_time_ms_ >= min_on_ms_` — перейде в IDLE негайно, хоча `comp_on_time_ms_` рахувався ДО defrost

**Не катастрофа (рідко створює проблему), але state machine inconsistency — SEV-2 правильна.**

---

### BUG-006: Defrost enter_phase() no re-entry guard
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `defrost_module.cpp:130-131`:
```cpp
void DefrostModule::enter_phase(Phase p) {
    phase_ = p;
    phase_timer_ms_ = 0;  // Скидається завжди
```
- Немає `if (phase_ == p) return;`
- Наслідок: якщо `enter_phase(Phase::ACTIVE)` викликається повторно — таймер скидається, defrost подовжується нескінченно
- Практично: через логіку коду це не повинно відбуватись (кожна фаза переходить в іншу), але **захисту немає**

**Підтверджено, але практичний ризик низький. SEV-2 або SEV-3.**

---

### BUG-007: High temp alarm blocked during ALL defrost phases
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `protection_module.cpp:128`:
```cpp
if (defrost_active) {
    high_temp_.pending = false;
    high_temp_.pending_ms = 0;
    return;
}
```
- `defrost.active` = true для УСІХ фаз (STABILIZE, VALVE_OPEN, ACTIVE, EQUALIZE, DRIP, FAD)
- За spec: HAL повинен блокуватись лише в ACTIVE фазі (коли тен/ГГ гріє)
- В фазах DRIP, FAD — температура вже нормальна, але High Temp alarm все одно заблоковано

**Підтверджено. SEV-2 правильна.**

---

### BUG-008: Demand defrost без мінімального інтервалу
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `defrost_module.cpp:331-334`:
```cpp
bool DefrostModule::check_demand_trigger() {
    if (!sensor2_ok_) return false;
    return evap_temp_ < demand_temp_;
}
```
- Перевіряється ТІЛЬКИ температура, без перевірки `interval_timer_ms_`
- Якщо `evap_temp_` стабільно нижче `demand_temp_` — defrost спрацює одразу після завершення попереднього (0 інтервал)
- Єдиний захист: оптимізація (рядок 317-321) пропускає defrost якщо `evap_temp_ > end_temp_`, але це не допоможе якщо `demand_temp_ < end_temp_`

**Підтверджено. SEV-2 правильна.**

---

### BUG-009: Compressor timer based on request, not actual
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `thermostat_module.cpp:216-220`:
```cpp
if (compressor_on_) {
    comp_on_time_ms_ += dt_ms;
} else {
    comp_off_time_ms_ += dt_ms;
}
```
- `compressor_on_` = request flag (рядок 80-84), НЕ `equipment.compressor` (фактичний стан)
- При інтерлоці (heater+compressor → compressor OFF) — thermostat вважає компресор ON, але EM його вимкнув
- `comp_on_time_ms_` рахується неправильно → `min_on_time` захист не спрацьовує

**Підтверджено. SEV-2 правильна.**

---

### BUG-010: 3 consecutive timeouts — no alarm
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `defrost_module.cpp:399-401`:
```cpp
if (consecutive_timeouts_ >= 3) {
    ESP_LOGW(TAG, "3 consecutive timeouts — possible heater/sensor failure!");
}
```
- Лише лог, немає `state_set("defrost.heater_alarm", true)` або подібного
- Protection module не знає про цю проблему, WebUI не показує

**Підтверджено. SEV-2 правильна.**

---

### BUG-021: WS client list race condition
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `ws_service.cpp:57-78` → `add_client()` пише в `client_fds_[]` — викликається з httpd thread
- `ws_service.cpp:296-319` → `send_to_all()` читає `client_fds_[]` — з main loop (on_update)
- Немає мьютексу або atomic операцій на `client_fds_[]`
- Можливий data race: main loop читає fd, httpd thread змінює/додає

**Підтверджено. SEV-2 правильна.**

---

### BUG-022: PersistService dirty[] race condition
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `shared_state.cpp:52-53` → persist callback викликається ПОЗА mutex
- `persist_service.cpp:109` → `dirty_[i] = true;` — з контексту callback (будь-який thread що зробив set())
- `persist_service.cpp:119-123` → `dirty_[i]` читається з main loop (on_update)
- Запис `bool` — зазвичай atomic на ESP32, але формально data race

**Підтверджено. Практичний ризик мінімальний (bool write/read зазвичай atomic на ARM), SEV-2 може бути занадто високою — SEV-3 теж прийнятна.**

---

### BUG-023: POST /api/settings — heuristic type detection
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- HTTP handler використовує евристичне визначення типу:
  - Є крапка → float
  - Починається з t/f → bool
  - Інакше → int
- Не перевіряє `meta->type` для визначення типу
- `"0"` для float поля → збережеться як int замість float
- `"10.0"` для int поля → збережеться як float замість int

**Підтверджено. SEV-2 правильна.**

---

### BUG-024: init_all() always returns true
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `module_manager.cpp:84` → `return true;`
- Навіть якщо CRITICAL модуль (Equipment) → `State::ERROR` — ігнорується
- `main.cpp` не перевіряє результат init_all()
- Система працює з неініціалізованими CRITICAL модулями → невизначена поведінка

**Підтверджено. SEV-2 правильна.**

---

## SEV-3 — MODERATE (8 багів)

### BUG-011: WS broadcast buffer 2048 overflow risk
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `ws_service.cpp:238` → `char buf[2048]`
- 69 state keys × ~30 bytes/key = ~2070 bytes мінімум
- Safety margin (рядок 31): `if (ctx->remaining() < 64) return;` — ключі в кінці мовчки зникають
- Аналогічна проблема в `http_service.cpp` → `char buf[2048]`

**Підтверджено. На поточній кількості ключів (69) — на межі. Додавання нових ключів гарантовано зламає. SEV-3 правильна, але може бути SEV-2 при зростанні.**

---

### BUG-012: NVS key positional coupling
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `persist_service.cpp:28` → `snprintf(out, out_size, "p%u", (unsigned)index)`
- NVS ключ "p5" = index 5 в STATE_META
- При зміні порядку/додаванні в manifest → індекси зміщуються → NVS читає чужі значення
- Без автоматичної міграції — silent data corruption після firmware update

**Підтверджено. SEV-3 правильна.**

---

### BUG-013: Duplicated read helpers
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- Однакові `read_float()`, `read_bool()`, `read_int()` в:
  - `thermostat_module.cpp:32-51`
  - `protection_module.cpp:32-51`
  - `defrost_module.cpp:30-49`
- 20 рядків × 3 = 60 рядків дублікату

**Підтверджено. Code smell, не баг. SEV-3/SEV-4.**

---

### BUG-014: PersistService 5s debounce, no immediate write
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `persist_service.h:32` → `DEBOUNCE_MS = 5000`
- Немає `flush()` або `save_now()` API
- При перезавантаженні ≤5 сек після зміни — значення втрачається

**Підтверджено. SEV-3 правильна.**

---

### BUG-015: comp_off_time_ms_ magic number 999999
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `thermostat_module.h:113` → `uint32_t comp_off_time_ms_ = 999999;`
- Ідея: дозволити перший старт компресора без очікування min_off_time
- Працює коректно, але "magic number" — код якість

**Підтверджено. SEV-4 скоріше за SEV-3.**

---

### BUG-016: FAD phase detection via string compare
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `thermostat_module.cpp:203-206`:
```cpp
const auto* sp = etl::get_if<etl::string<32>>(&phase_val.value());
if (sp && *sp == "fad") {
    defrost_fad_ = true;
}
```
- StringValue = `etl::string<32>`, порівняння `*sp == "fad"` — виклик strcmp
- Працює, але крихке: зміна "fad" → "FAD" в defrost_module зламає

**Підтверджено. SEV-3 правильна.**

---

### BUG-017: SharedState version++ on unchanged value
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `shared_state.cpp:43-44`:
```cpp
map_[key] = value;
version_++;  // Завжди! Навіть якщо value не змінився
```
- `changed` (рядки 37-41) використовується ТІЛЬКИ для persist callback
- `version_++` безумовний → WS broadcast кожну секунду навіть без змін
- При 69 ключах × 100ms loop = ~690 set() / сек → ~690 version increments

**Підтверджено. Зайвий трафік WS, але не критично. SEV-3 правильна.**

---

### BUG-018: SharedState overflow silent
**Вердикт: ✅ ПІДТВЕРДЖЕНО (з уточненням)**

- `shared_state.cpp:30-33`:
```cpp
if (map_.full() && !map_.count(key)) {
    ESP_LOGW(TAG, "State full, cannot add key: %s", key.c_str());
    xSemaphoreGive(mutex_);
    return false;
}
```
- Overflow detection ІСНУЄ (ESP_LOGW + return false)
- Але: жоден модуль не перевіряє return value `set()` → silent failure
- `MODESP_MAX_STATE_ENTRIES = 96`, поточних ключів 69 + ~10 system = ~79 → запас 17

**Підтверджено. Проблема в ігноруванні return value, не у відсутності детекції. SEV-3 правильна.**

---

## SEV-4 — LOW (2 баги)

### BUG-019: Asymmetric sensor error handling
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `equipment_module.cpp:131-137` → sensor1: `if (has_air_temp_)` guard при помилці читання
- `equipment_module.cpp:148-149` → sensor2: немає guard, одразу `sensor2_ok = false`
- Sensor1 зберігає останнє значення при помилці, sensor2 — ні

**Підтверджено. Мінорна асиметрія, sensor2 необов'язковий. SEV-4 правильна.**

---

### BUG-020: malloc() in WS broadcast hot path
**Вердикт: ✅ ПІДТВЕРДЖЕНО**

- `ws_service.cpp:309-310`:
```cpp
auto* ctx = static_cast<AsyncSendCtx*>(
    malloc(sizeof(AsyncSendCtx) + len));
```
- Кожен broadcast × кожен клієнт = malloc + free
- За arch: `httpd_queue_work()` вимагає context pointer що freed в callback
- Альтернатива: ring buffer або pool — складніше

**Підтверджено. Architectural limitation ESP-IDF httpd_queue_work, не баг per se. SEV-4 правильна.**

---

## 🆕 НОВІ БАГИ (виявлені під час верифікації)

### NEW-001: Equipment on_message — wrong message ID (SEV-2)

**Файл:** `equipment_module.cpp:103`

```cpp
if (msg.get_message_id() == 1) {  // ПОМИЛКА! Це SYSTEM_SHUTDOWN, не SAFE_MODE
```

- `types.h:86` → `SYSTEM_SHUTDOWN = 1`
- `types.h:92` → `SYSTEM_SAFE_MODE = 7`
- Equipment реагує на **SYSTEM_SHUTDOWN** замість **SYSTEM_SAFE_MODE**!
- Safe Mode ніколи не вимикає outputs через Equipment

**Для порівняння:** thermostat правильно використовує `modesp::msg_id::SYSTEM_SAFE_MODE` (рядок 458)

**Фікс:**
```cpp
if (msg.get_message_id() == modesp::msg_id::SYSTEM_SAFE_MODE) {
```

---

### NEW-002: MAX_PERSIST_KEYS = 16 — занадто мало (SEV-1 CRITICAL!)

**Файл:** `persist_service.h:33`

```cpp
static constexpr size_t MAX_PERSIST_KEYS = 16;
```

- `state_meta.h:55` → `STATE_META_COUNT = 33`
- Persist keys з persist=true: **19 штук** (рахуємо з state_meta.h)
- Цикли в persist_service обрізані на `i < MAX_PERSIST_KEYS`:
  - `persist_service.cpp:103` → `on_state_changed()` — сканує тільки індекси 0..15
  - `persist_service.cpp:119` → `on_update()` — dirty check тільки 0..15
  - `persist_service.cpp:140` → `flush_to_nvs()` — пише тільки 0..15

**Наслідок:** Persist ключі з індексами ≥16 **НІКОЛИ НЕ ЗБЕРІГАЮТЬСЯ** в NVS при runtime змінах!

Зокрема:
- `defrost.interval_timer` (idx ≥16) — **НЕ зберігається** при перезавантаженні
- `defrost.defrost_count` (idx ≥16) — **НЕ зберігається** при перезавантаженні
- Це прямо суперечить документації в CLAUDE.md: "2 runtime persist (interval_timer, defrost_count)"

**Фікс:**
```cpp
static constexpr size_t MAX_PERSIST_KEYS = 48;  // або STATE_META_COUNT
```

---

### NEW-003: restore_from_nvs() vs dirty/flush — asymmetric limits (SEV-2)

**Файл:** `persist_service.cpp`

- `restore_from_nvs()` (рядок 52) ітерує `i < gen::STATE_META_COUNT` — **повний діапазон** ✅
- `on_state_changed()` (рядок 103) ітерує `i < STATE_META_COUNT && i < MAX_PERSIST_KEYS` — **обрізано** ❌
- `on_update()` / `flush_to_nvs()` — аналогічно обрізано на 16

**Наслідок:**
- При boot: відновлення з NVS працює для ВСІХ persist ключів (якщо вони там є)
- При runtime зміні ключа з індексом ≥16 → dirty flag НЕ встановлюється → зміна НІКОЛИ не зберігається в NVS
- Settings змінені через WebUI для ключів idx≥16 **втрачаються при перезавантаженні**

---

## 📊 ЗВЕДЕНА ТАБЛИЦЯ

| Bug | Severity | Вердикт | Примітка |
|-----|----------|---------|----------|
| BUG-001 | SEV-1 | ✅ ПІДТВЕРДЖЕНО | "23 chars" → коректно "24 chars", але проблема реальна |
| BUG-002 | SEV-1 | ✅ ПІДТВЕРДЖЕНО | Критично для hot gas defrost |
| BUG-003 | SEV-1 | ⚠️ BY DESIGN | Знизити до SEV-2 (задокументовано як Phase 10+) |
| BUG-004 | SEV-1 | ✅ ПІДТВЕРДЖЕНО | Критично — можливе пошкодження компресора |
| BUG-005 | SEV-2 | ✅ ПІДТВЕРДЖЕНО | State inconsistency |
| BUG-006 | SEV-2 | ✅ ПІДТВЕРДЖЕНО | Низький практичний ризик |
| BUG-007 | SEV-2 | ✅ ПІДТВЕРДЖЕНО | HAL alarm masked в non-heating phases |
| BUG-008 | SEV-2 | ✅ ПІДТВЕРДЖЕНО | Demand без мін. інтервалу |
| BUG-009 | SEV-2 | ✅ ПІДТВЕРДЖЕНО | Timer mismatch при інтерлоці |
| BUG-010 | SEV-2 | ✅ ПІДТВЕРДЖЕНО | 3 timeouts → лише лог |
| BUG-011 | SEV-3 | ✅ ПІДТВЕРДЖЕНО | На межі при 69 ключах |
| BUG-012 | SEV-3 | ✅ ПІДТВЕРДЖЕНО | Positional NVS coupling |
| BUG-013 | SEV-3 | ✅ ПІДТВЕРДЖЕНО | Code duplication |
| BUG-014 | SEV-3 | ✅ ПІДТВЕРДЖЕНО | No immediate flush |
| BUG-015 | SEV-3 | ✅ ПІДТВЕРДЖЕНО | Magic number (скоріше SEV-4) |
| BUG-016 | SEV-3 | ✅ ПІДТВЕРДЖЕНО | String-based phase compare |
| BUG-017 | SEV-3 | ✅ ПІДТВЕРДЖЕНО | Redundant version increments |
| BUG-018 | SEV-3 | ✅ ПІДТВЕРДЖЕНО | Overflow detected but return ignored |
| BUG-019 | SEV-4 | ✅ ПІДТВЕРДЖЕНО | Asymmetric sensor handling |
| BUG-020 | SEV-4 | ✅ ПІДТВЕРДЖЕНО | ESP-IDF API limitation |
| BUG-021 | SEV-2 | ✅ ПІДТВЕРДЖЕНО | Data race on client_fds_ |
| BUG-022 | SEV-2 | ✅ ПІДТВЕРДЖЕНО | Формально race, практично safe |
| BUG-023 | SEV-2 | ✅ ПІДТВЕРДЖЕНО | Type heuristic vs meta->type |
| BUG-024 | SEV-2 | ✅ ПІДТВЕРДЖЕНО | init_all() завжди true |
| **NEW-001** | **SEV-2** | 🆕 **НОВИЙ** | **Equipment: msg_id 1 замість 7 (SAFE_MODE)** |
| **NEW-002** | **SEV-1** | 🆕 **НОВИЙ** | **MAX_PERSIST_KEYS=16 < 19 persist keys** |
| **NEW-003** | **SEV-2** | 🆕 **НОВИЙ** | **restore OK, dirty/flush обрізано на 16** |

---

## Рекомендований пріоритет фіксів

### Негайно (SEV-1):
1. **NEW-002** — `MAX_PERSIST_KEYS = 48` (1 рядок)
2. **BUG-001** — `MODESP_MAX_KEY_LENGTH = 32` (1 рядок, але потрібна ревалідація всіх ключів)
3. **BUG-002** — Cache `defrost_type_` при `start_defrost()`, не перечитувати в `finish_active_phase()`
4. **BUG-004** — Додати COd затримку перед defrost або дозволити `update_cond_fan()` працювати

### Наступний спринт (SEV-2):
5. **NEW-001** — `msg.get_message_id() == modesp::msg_id::SYSTEM_SAFE_MODE` (1 рядок)
6. **BUG-024** — `init_all()` → return false якщо CRITICAL module failed
7. **BUG-007** — Передавати `defrost.phase` замість `defrost.active` в Protection
8. **BUG-023** — Використовувати `meta->type` замість евристики в POST /api/settings
9. **BUG-009** — Читати `equipment.compressor` замість `compressor_on_`
10. **BUG-021** — Atomic або mutex на `client_fds_[]`

---

## Сесія фіксів #1 (попередній агент, 2026-02-20)

| Коміт | Баг | Опис фіксу |
|-------|-----|-------------|
| `ac4f362` | NEW-002, NEW-003 | `MAX_PERSIST_KEYS = 48` — persist keys з індексами >=16 тепер зберігаються |
| `de57fe1` | BUG-001, BUG-011 | `MODESP_MAX_KEY_LENGTH = 32`, буфери WS/HTTP 2048→3072 |
| `7b4e362` | BUG-002 | Cache `defrost_type_` в `active_defrost_type_` при `start_defrost()` |
| `d1aa4d6` | BUG-004 | COd delay працює при defrost — `update_cond_fan(dt_ms)` замість `request_cond_fan(false)` |
| `e664507` | NEW-001 | Equipment `on_message()` — `msg_id::SYSTEM_SAFE_MODE` (7) замість literal `1` |

**Верифікація:** 5/5 CORRECT, без регресій.

---

## Сесія фіксів #2 (2026-02-20)

### FIX: NEW-004 — JSON parsing bounds check (SEV-1)

**Файл:** `components/modesp_net/src/http_service.cpp`
**Рядки:** 241, 320, 580

**Проблема:** Цикл `for (int i = 1; i < r - 1; i += 2)` міг вийти за межі масиву `tokens[]` при непарній кількості токенів або malformed JSON. Доступ до `tokens[i+1]` без перевірки `i+1 < r`.

**Фікс:** Замінено умову циклу на `i + 1 < r` в усіх 3 JSON parsing loops:
- `handle_post_settings()` (рядок 241)
- `handle_post_wifi()` (рядок 320)
- `handle_post_time()` (рядок 580)

```cpp
// Було:
for (int i = 1; i < r - 1; i += 2)
// Стало:
for (int i = 1; i + 1 < r; i += 2)
```

---

### FIX: BUG-008 — Demand defrost без мінімального інтервалу (SEV-2)

**Файл:** `modules/defrost/src/defrost_module.cpp`
**Функція:** `check_demand_trigger()`

**Проблема:** Demand trigger перевіряв тільки `evap_temp_ < demand_temp_` без перевірки мінімального інтервалу. Якщо T_evap стабільно нижче demand_temp_, defrost запускався одразу після завершення попереднього.

**Фікс:** Додано перевірку мінімального інтервалу (25% від повного):
```cpp
if (interval_timer_ms_ < interval_ms_ / 4) return false;
```

Це гарантує що між defrost циклами пройде щонайменше 25% від налаштованого інтервалу (наприклад, 2 години при 8-годинному інтервалі).

---

### FIX: BUG-010 — 3 consecutive timeouts без alarm (SEV-2)

**Файл:** `modules/defrost/src/defrost_module.cpp`
**Функція:** `update_active_phase()`

**Проблема:** При 3 послідовних timeout defrost виводив тільки `ESP_LOGW`, не публікуючи alarm. Protection і WebUI не знали про проблему з тенів/датчиком.

**Фікс:**
1. При `consecutive_timeouts_ >= 3` публікується `defrost.heater_alarm = true`
2. При успішному завершенні по температурі — `defrost.heater_alarm = false`
3. Ініціалізація `defrost.heater_alarm = false` в `on_init()`

---

### FIX: BUG-024 — init_all() завжди повертає true (SEV-2)

**Файл:** `components/modesp_core/src/module_manager.cpp`
**Функція:** `init_all()`

**Проблема:** `init_all()` повертав `true` навіть якщо CRITICAL модуль (Equipment, PersistService) не пройшов ініціалізацію. Система працювала з модулями в стані ERROR.

**Фікс:** Після ініціалізації всіх модулів — перевірка CRITICAL модулів. Якщо хоча б один в стані ERROR, повертає `false`:
```cpp
bool critical_ok = true;
for (auto* module : modules_) {
    if (module->priority() == ModulePriority::CRITICAL &&
        module->state_ == BaseModule::State::ERROR) {
        ESP_LOGE(TAG, "CRITICAL module '%s' failed init!", module->name());
        critical_ok = false;
    }
}
return critical_ok;
```

---

### FIX: BUG-009 — Compressor timer базується на request, не actual (SEV-2)

**Файл:** `modules/thermostat/src/thermostat_module.cpp`
**Функція:** `on_update()`

**Проблема:** Таймери `comp_on_time_ms_` / `comp_off_time_ms_` рахувались за `compressor_on_` (request flag), а не за фактичним станом `equipment.compressor`. При інтерлоці (heater+compressor → compressor OFF) thermostat вважав компресор ON, хоча EM його вимкнув.

**Фікс:** Таймери рахуються за фактичним станом компресора:
```cpp
bool comp_actual = read_bool("equipment.compressor");
if (comp_actual) {
    comp_on_time_ms_ += dt_ms;
} else {
    comp_off_time_ms_ += dt_ms;
}
```

---

### FIX: BUG-007 — HAL alarm blocked у всіх фазах defrost (SEV-2)

**Файл:** `modules/protection/src/protection_module.cpp`
**Функція:** `on_update()`

**Проблема:** High Temp alarm блокувався при `defrost.active = true` — для ВСІХ фаз (включаючи DRIP і FAD де нагріву немає). За spec HAL alarm повинен блокуватися тільки під час нагріву.

**Фікс:** Замість `defrost.active` перевіряємо `defrost.phase` — блокуємо тільки в heating-фазах:
```cpp
bool defrost_heating = false;
if (defrost) {
    auto phase_val = state_get("defrost.phase");
    if (phase_val.has_value()) {
        const auto* sp = etl::get_if<etl::string<32>>(&phase_val.value());
        if (sp && (*sp == "active" || *sp == "stabilize" ||
                   *sp == "valve_open" || *sp == "equalize")) {
            defrost_heating = true;
        }
    }
}
update_high_temp(air_temp, sensor1_ok, defrost_heating, dt_ms);
```

---

### FIX: BUG-017 — version++ при незміненому значенні (SEV-3)

**Файл:** `components/modesp_core/src/shared_state.cpp`
**Функція:** `set()`

**Проблема:** `version_++` виконувався безумовно при кожному `set()`, навіть якщо значення не змінилось. При 69 ключах × 100ms loop = ~690 version increments/сек → зайвий WS broadcast трафік.

**Фікс:** `version_++` тільки при реальній зміні значення (`changed == true`):
```cpp
map_[key] = value;
if (changed) {
    version_++;
}
```

Змінна `changed` вже обчислювалась для persist callback, тепер використовується і для version.

---

## Оновлена зведена таблиця

| Bug | Severity | Статус | Фікс |
|-----|----------|--------|------|
| BUG-001 | SEV-1 | **FIXED** (сесія #1) | Key length 24→32 |
| BUG-002 | SEV-1 | **FIXED** (сесія #1) | Cache defrost_type at start |
| BUG-003 | SEV-1→2 | BY DESIGN | Phase 10+ |
| BUG-004 | SEV-1 | **FIXED** (сесія #1) | COd delay during defrost |
| BUG-005 | SEV-2 | OPEN | State inconsistency during defrost |
| BUG-006 | SEV-2 | OPEN | enter_phase() no re-entry guard |
| BUG-007 | SEV-2 | **FIXED** (сесія #2) | HAL alarm → heating phases only |
| BUG-008 | SEV-2 | **FIXED** (сесія #2) | Demand defrost min interval 25% |
| BUG-009 | SEV-2 | **FIXED** (сесія #2) | Compressor timer → equipment.compressor |
| BUG-010 | SEV-2 | **FIXED** (сесія #2) | defrost.heater_alarm published |
| BUG-011 | SEV-3 | **FIXED** (сесія #1) | Buffers 2048→3072 |
| BUG-012 | SEV-3 | OPEN | NVS positional keys |
| BUG-013 | SEV-3 | OPEN | read helpers duplication |
| BUG-014 | SEV-3 | OPEN | No immediate NVS flush |
| BUG-015 | SEV-3 | OPEN | Magic number 999999 |
| BUG-016 | SEV-3 | OPEN | String-based phase compare |
| BUG-017 | SEV-3 | **FIXED** (сесія #2) | version++ only on change |
| BUG-018 | SEV-3 | OPEN | set() return value ignored |
| BUG-019 | SEV-4 | BY DESIGN | Asymmetric sensor handling |
| BUG-020 | SEV-4 | OPEN | malloc in WS broadcast |
| BUG-021 | SEV-2 | OPEN | WS client_fds_ race |
| BUG-022 | SEV-2 | OPEN | PersistService dirty[] race |
| BUG-023 | SEV-2 | OPEN | POST type heuristic |
| BUG-024 | SEV-2 | **FIXED** (сесія #2) | init_all() → false on CRITICAL fail |
| NEW-001 | SEV-2 | **FIXED** (сесія #1) | SAFE_MODE msg_id |
| NEW-002 | SEV-1 | **FIXED** (сесія #1) | MAX_PERSIST_KEYS 48 |
| NEW-003 | SEV-2 | **FIXED** (сесія #1) | restore/flush consistent limits |
| NEW-004 | SEV-1 | **FIXED** (сесія #2) | JSON bounds check |

**Підсумок:** 14/27 FIXED, 11 OPEN, 2 BY DESIGN.

---

## Залишки (OPEN баги)

### Потребують уваги (SEV-2):
- **BUG-005** — Thermostat state не скидається при defrost (state machine inconsistency)
- **BUG-006** — enter_phase() без re-entry guard (низький практичний ризик)
- **BUG-021** — WS client_fds_ race condition (потребує mutex)
- **BUG-022** — PersistService dirty[] формально race (практично safe на ARM)
- **BUG-023** — POST /api/settings type heuristic замість meta->type

### Технічний борг (SEV-3/4):
- **BUG-012** — NVS positional keys coupling
- **BUG-013** — read_float/bool/int дублюються в 3 модулях
- **BUG-014** — Немає immediate NVS flush API
- **BUG-015** — Magic number 999999 (comp_off_time_ms_)
- **BUG-016** — String-based phase detection ("fad")
- **BUG-018** — set() return value ніхто не перевіряє
- **BUG-020** — malloc() в WS broadcast (ESP-IDF API limitation)

---

## Changelog
- 2026-02-20 — Створено. Верифіковано 24/24 баги, знайдено 3 нові (NEW-001, NEW-002, NEW-003).
- 2026-02-20 — Сесія #1: 5 фіксів (BUG-001,002,004,011 + NEW-001,002,003). Верифіковано: 5/5 CORRECT.
- 2026-02-20 — Сесія #2: 7 фіксів (NEW-004, BUG-007,008,009,010,017,024). 14/27 total fixed.
