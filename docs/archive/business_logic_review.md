# ModESP v4 — Ревью бізнес-логіки

> Дата: 2026-02-19
> Ревізія: Phase 9.4 (Equipment + Protection + Thermostat + Defrost)
> Еталон: `docs/controller_spec_v3.txt` (Версія 2.0, 2025)

Цей документ порівнює **реалізацію C++** з **специфікацією spec_v3** і виявляє
невідповідності, пропущену логіку та потенційні баги.

---

## Зведена таблиця відповідностей

| Вимога spec_v3 | Реалізовано? | Проблема |
|----------------|:---:|----------|
| Thermostat ON: T ≥ SP+diff | ✅ | — |
| Thermostat OFF: T ≤ SP, після cOt | ✅ | — |
| min_off_time (cFt) перед стартом | ✅ | — |
| Startup delay після boot | ✅ | — |
| Safety Run при ERR1 | ✅ | Не перевіряє blocking alarms |
| Блокування старту при DFR/DRP/FAD | ⚠️ | Частково — див. B1 |
| Блокування старту при active alarms | ❌ | Див. B2 |
| Зупинка компресора при ініціації defrost | ❌ | Див. B3 |
| Зупинка компресора при захисній аварії | ❌ | Див. B4 |
| Evap fan mode 0/1/2 | ✅ | — |
| Evap fan OFF при DFR + DRP + FAD | ⚠️ | Частково — див. B5 |
| Cond fan delayed OFF (COd) | ✅ | — |
| Cond fan OFF при defrost (крім FAD) | ✅ | — |
| Cond fan natural defrost delayed OFF | ❌ | Див. B6 |
| High Temp alarm delayed (dAd) | ✅ | — |
| High Temp blocked in DFR/DRP | ⚠️ | Блокує всі фази defrost, не тільки DFR/DRP — див. B7 |
| Low Temp alarm delayed (dAd) | ✅ | — |
| ERR1/ERR2 instant | ✅ | — |
| Door alarm delayed | ✅ | — |
| Manual reset | ✅ | — |
| Defrost 3 types (0/1/2) | ✅ | — |
| Defrost 7-phase hot gas | ✅ | — |
| Defrost timer/demand/combo/manual | ✅ | — |
| Defrost skip if evap clean | ✅ | — |
| Defrost termination temp + safety timer | ✅ | — |
| Defrost consecutive timeout alarm | ⚠️ | Лічильник є, але alarm не генерується — див. B8 |
| NVS persist interval_timer | ✅ | — |
| FAD compressor+cond ON, evap OFF | ✅ | — |
| Interlocks heater↔compressor | ✅ | — |
| Interlocks heater↔hg_valve | ✅ | — |
| Demand defrost мін. інтервал | ❌ | Див. B9 |

---

## Виявлені проблеми бізнес-логіки

### B1: Thermostat не зупиняється повністю при defrost

**Spec §2.3:**
> «Умови запуску: Стан ≠ DFR, DRP, FAD»

**Реалізація (thermostat_module.cpp:230-244):**
```cpp
// 6. Defrost active — thermostat не керує (крім FAD)
if (defrost_active_ && !defrost_fad_) {
    request_compressor(false);
    request_evap_fan(false);
    request_cond_fan(false);
    return;
}
// FAD phase: ...
if (defrost_fad_) {
    request_evap_fan(false);
    return;
}
```

**Проблема:** При FAD thermostat **не вимикає свій request_compressor**. Він просто
робить `return` без зміни стану. Якщо thermostat був в COOLING до defrost → його
`thermostat.req.compressor` залишається `true` в SharedState. EM бере defrost.req.*
як пріоритет (бо defrost.active=true), але якщо defrost завершиться між циклами —
один цикл thermostat request може "проскочити" без перевірки температури.

Також: thermostat не скидає свій state machine на IDLE при вході в defrost.
Він залишається в COOLING і продовжує рахувати comp_on_time_ms_, хоча компресор
фактично вимкнений defrost'ом (evap OFF, все OFF). При виході з defrost —
comp_on_time_ms_ хибно великий.

**Рішення:** При `defrost_active_ && phase != FAD` → примусово `enter_state(IDLE)`.
При FAD → також `request_compressor(false)` (Defrost сам шле req через EM).

---

### B2: Thermostat ігнорує blocking alarms при запуску

**Spec §2.3:**
> «Умови запуску: Немає активних аварій, що блокують запуск»

**Реалізація (thermostat_module.cpp:222-228):**
```cpp
if (protection_lockout_) {
    request_compressor(false);
    ...
    return;
}
```

**Проблема:** Thermostat перевіряє тільки `protection.lockout` (який ЗАВЖДИ false —
зарезервований). Він **не читає** `protection.alarm_active` і не блокує запуск
компресора при активних аваріях (high_temp, low_temp, err1). По суті, Protection
модуль зараз — чисто інформаційний, не впливає на поведінку системи.

**Рішення:**
- **Варіант А:** Protection встановлює `protection.lockout = true` при критичних
  аваріях (ERR1, або multiple alarms). Потребує визначення: які аварії блокують,
  а які тільки сигналізують.
- **Варіант Б:** Thermostat сам читає `protection.alarm_active` і блокує старт.
  Менш чисто архітектурно (thermostat знає про protection internals).

---

### B3: Thermostat не реагує на ініціацію defrost як trigger зупинки

**Spec §2.3:**
> «Умови зупинки: Ініційовано відтайку (перехід у стан DFR)»

**Реалізація:** Thermostat дізнається про defrost тільки коли `defrost.active = true`
вже встановлений. Але між рішенням Defrost "я починаю" і фактичним enter_phase(ACTIVE)
може пройти 1 цикл. У цей момент thermostat ще в COOLING → шле req.compressor=true →
EM застосовує → relay ON → потім defrost вимикає.

Це не критично (1 цикл = 100ms), але для hot gas defrost — компресор має бути OFF
**до** відкриття клапана (Phase 1 = STABILIZE). Якщо thermostat встигне послати
"компресор ON" на тому ж циклі коли defrost починається — EM поставить компресор ON
(бо defrost ще не active), а наступний цикл defrost скаже "все OFF" для стабілізації.

**Рішення:** Defrost має встановлювати `defrost.active = true` **до** входу в першу
фазу, або Equipment має per-cycle ordering guarantee (Equipment update ПІСЛЯ defrost).
Зараз Equipment оновлюється ПЕРШИМ (priority=0), тобто він читає СТАРІ requests.
Це правильно — проблема мінімальна, але варто документувати цю one-cycle delay.

---

### B4: Thermostat не зупиняється при захисній аварії

**Spec §2.3:**
> «Умови зупинки: Спрацювала захисна аварія»

**Реалізація:** Як описано в B2, thermostat реагує тільки на `protection.lockout`
(завжди false). Навіть ERR1 (sensor1_alarm) не зупиняє компресор через Protection →
тільки через Safety Run (коли sensor1_ok стає false). Але якщо є alarm_active від
high_temp — компресор **не зупиняється**, продовжує працювати.

По специфікації: high temp alarm має зупиняти компресор (бо це означає проблему
з системою — компресор не охолоджує, працює дарма, зношується).

**Рішення:** Визначити матрицю "alarm → action":
- ERR1 → Safety Run (вже реалізовано через sensor1_ok)
- High Temp → зупинити компресор (система не охолоджує, немає сенсу гнати)
- Low Temp → зупинити компресор (переохолодження, товар замерзає)
- ERR2 → інформаційна (не блокує)
- Door → інформаційна (не блокує)

---

### B5: Evap fan не зупиняється під час DRIP фази (для thermostat-managed fan)

**Spec §2.5:**
> «Вентилятор ЗАВЖДИ зупиняється під час відтайки (DFR) та дренажу (DRP)»
> «Вентилятор ЗАВЖДИ зупиняється під час затримки FAD»

**Реалізація (thermostat_module.cpp:350-355):**
```cpp
void ThermostatModule::update_evap_fan() {
    if (defrost_active_) {
        request_evap_fan(false);
        return;
    }
    ...
}
```

Thermostat вимикає evap fan при `defrost_active_ = true`. Defrost встановлює
`defrost.active = true` для фаз STABILIZE → VALVE_OPEN → ACTIVE → EQUALIZE.

**Але DRIP і FAD:** `defrost.active` залишається true (enter_phase() ставить
`defrost.active = p != Phase::IDLE`). Тобто DRIP і FAD мають active=true →
thermostat вимикає evap fan. Це **правильно** для DRIP і FAD.

**Однак є нюанс:** Defrost сам шле `defrost.req.evap_fan = false` для всіх фаз
крім... ніяких (він ніколи не шле evap_fan=true). А EM при `defrost_active`
бере `def_evap_fan` — яке завжди false. Тобто evap fan OFF при будь-якому
defrost — **через EM**, не через Thermostat. Подвійний механізм — працює, але
заплутаний: Thermostat і EM обидва вимикають fan, по різних шляхах.

**Рішення:** Документувати або спростити — один механізм замість двох.

---

### B6: Cond fan при природній відтайці (dFT=0) — немає delayed OFF

**Spec §3.1.1:**
> «Вентилятор конденсатора: ВИМК (з затримкою COd)»

При природній відтайці (dFT=0) defrost шле `set_requests(false, false, false, false, false)` —
тобто `defrost.req.cond_fan = false`. EM при `defrost_active` бере `def_cond_fan = false`.

**Проблема:** Якщо до defrost компресор + конд.вент. працювали, при вході в natural defrost
конд.вент. **різко вимикається** (EM ставить out.cond_fan=false). Spec каже затримка COd
повинна бути. Але затримка COd реалізована в **Thermostat** (update_cond_fan), а при
defrost_active Thermostat **не виконує** update_cond_fan — він робить `return` раніше.

**Результат:** Конд. вент. вимикається миттєво замість затримки COd при старті defrost.

**Рішення:** Або Defrost/EM самі реалізують delayed OFF для cond_fan, або Thermostat
перед return'ом виконує cond_fan delay навіть під час defrost.

---

### B7: High Temp alarm блокується у ВСІХ фазах defrost, не тільки DFR/DRP

**Spec §1.3 + §2.7:**
> «Під час станів DFR та DRP аварія по верхній температурі примусово блокується»
> «Стан контролера: DRP — аварія по верхній температурі заблокована»

**Реалізація (protection_module.cpp:128-133):**
```cpp
if (defrost_active) {
    high_temp_.pending = false;
    high_temp_.pending_ms = 0;
    return;
}
```

**Проблема:** Protection блокує high temp alarm при `defrost.active = true`.
Defrost ставить `active = true` для ВСІХ фаз крім IDLE (STABILIZE, VALVE_OPEN,
ACTIVE, EQUALIZE, DRIP, FAD). Але spec каже блокувати тільки в DFR і DRP.

Фаза **FAD** — компресор працює, охолоджує. Якщо в FAD T_air > HAL — це реальна
проблема (компресор не охолоджує камеру, а лише випарник). Alarm мав би спрацювати.

Фаза **EQUALIZE** — все вимкнено, тиск падає. T_air може рости. Блокування
можливо виправдане (коротка фаза 1-2 хв), але spec не вказує це явно.

**Рішення:** Protection має перевіряти конкретні фази defrost (DFR=ACTIVE, DRP=DRIP),
а не просто `defrost.active`. Потрібен або `defrost.phase` string check, або
explicit `defrost.block_high_alarm` bool key.

---

### B8: 3 consecutive timeout defrost — лічильник є, alarm немає

**Spec §3.3:**
> «Три таких завершення підряд → аварійна сигналізація»

**Реалізація (defrost_module.cpp:399-401):**
```cpp
if (consecutive_timeouts_ >= 3) {
    ESP_LOGW(TAG, "3 consecutive timeouts — possible heater/sensor failure!");
}
```

**Проблема:** Тільки логування WARNING. Не створюється жодна аварія в Protection.
Немає state key для alarm, немає publish. Оператор на WebUI не побачить нічого —
тільки в Serial Monitor.

**Рішення:** Додати state key `defrost.timeout_alarm` або publish alarm через
Protection module. Варіанти:
- Defrost сам публікує `defrost.timeout_alarm = true` → Protection читає і реагує
- Defrost шле message через bus → Protection створює alarm
- Найпростіше: `defrost.consecutive_timeouts` вже публікується → Protection
  моніторить і при ≥3 → alarm

---

### B9: Demand defrost — немає мінімального інтервалу між відтайками

**Spec §3.2.2:**
> «Мінімальний інтервал між такими відтайками (захист від зациклення)»

**Реалізація (defrost_module.cpp:331-335):**
```cpp
bool DefrostModule::check_demand_trigger() {
    if (!sensor2_ok_) return false;
    return evap_temp_ < demand_temp_;
}
```

**Проблема:** Demand trigger перевіряє тільки `T_evap < dSS`. Немає жодної
перевірки мінімального часу з моменту останньої відтайки. Якщо випарник швидко
обмерзає (або датчик глючить), defrost запускатиметься кожні 8 годин (timer)
АБО кожен цикл (demand) — без обмеження знизу.

При combo mode (initiation=2) demand trigger може спрацювати через секунду
після завершення попередньої відтайки.

**Рішення:** Додати параметр `defrost.min_interval` (наприклад, 30 хвилин) і
перевіряти: `interval_timer_ms_ >= min_interval_ms_` перед demand trigger.

---

### B10: Thermostat comp_off_time_ms_ ініціалізується як 999999

**Реалізація (thermostat_module.h:113):**
```cpp
uint32_t comp_off_time_ms_ = 999999;   // Час з моменту OFF (великий для першого старту)
```

**Проблема:** Хак для першого старту: "компресор вже давно вимкнений, можна стартувати".
Але ця хитрість ламає startup_delay логіку — компресор може стартувати під час
STARTUP фази, бо comp_off_time_ms_ > min_off_ms_ з першого циклу.

Фактично це не відбувається тільки тому що state_ = STARTUP і update_regulation()
не викликається. Але якщо хтось змінить порядок перевірок — баг проявиться.

**Рішення:** Чисте рішення — не використовувати magic number. Замість цього:
`bool first_start_ = true;` → в update_regulation перевіряти: `if (first_start_)
{ first_start_ = false; comp_off_time_ms_ = min_off_ms_; }` або просто
ініціалізувати `comp_off_time_ms_ = min_off_ms_` в on_init() після sync_settings().

---

### B11: Thermostat не знає реальний стан компресора

**Проблема:** Thermostat трекає `compressor_on_` як свій внутрішній request,
а не фактичний стан relay. Якщо EM відхилив request (relay min_switch_ms ще
не пройшов, або defrost перебив) — thermostat думає що компресор ON, хоча
реально він OFF.

`comp_on_time_ms_` рахується від моменту request, а не від моменту фактичного ON.
`comp_off_time_ms_` аналогічно — від моменту request OFF.

**Наслідки:**
- min_on_time може закінчитись раніше ніж реальний мінімальний час роботи
- min_off_time може "проскочити" якщо relay rejected set()

**Рішення:** Thermostat має читати `equipment.compressor` (фактичний стан) для
трекінгу таймерів, а `compressor_on_` використовувати тільки як request state.

---

### B12: Equipment sensor1_ok — не скидається при повному disconnect

**Реалізація (equipment_module.cpp:126-137):**
```cpp
if (sensor_air_->read(temp)) {
    has_air_temp_ = true;
    state_set("equipment.sensor1_ok", true);
} else {
    if (has_air_temp_) {
        state_set("equipment.sensor1_ok", false);
    }
}
```

**Проблема:** Якщо `read()` провалюється з самого початку (датчик ніколи не
прочитався успішно) — `has_air_temp_ = false` → `sensor1_ok` **ніколи не стає
false** (бо умова `if (has_air_temp_)` не проходить). Тобто `sensor1_ok`
залишається в default стані (`false` з on_init), що правильно.

АЛЕ: якщо датчик прочитався 1 раз → `has_air_temp_ = true` → потім датчик
відключився → `sensor1_ok = false` → air_temp залишається **останнім прочитаним
значенням**. Thermostat читає `equipment.air_temp` = стара температура.
Він не знає що дані несвіжі (sensor1_ok = false перевіряється, переходить в
Safety Run — ОК). Але між "датчик здох" і "Protection поставить ERR1" —
thermostat використовує stale temperature.

Для 10Hz loop це 100ms затримки — мінімально. Але якщо sensor1_ok check
відбувається ПІСЛЯ regulation check (в одному cycle) — одне хибне рішення
можливе.

**Рішення:** Незначна проблема для 10Hz, але для чистоти — перевіряти
sensor1_ok ПЕРЕД regulation (вже робиться: рядок 259).

---

### B13: Natural defrost (dFT=0) — spec vs implementation конфлікт у вент. випарника

**Spec §3.1.1:**
> «Вентилятор випарника: ВИМК»

**Spec §3.1.1 (примітка):**
> «Альтернативна логіка: FAn=0 дозволяє залишити вентилятор увімкненим для обдуву
> випарника теплим повітрям камери (Air Defrost). Задається окремо.»

**Реалізація:** Defrost шле `req.evap_fan = false` завжди. Немає логіки для
"air defrost" варіанту. Якщо FAn=0 (постійно) — spec натякає що вент. може
працювати при dFT=0 для обдуву теплим повітрям.

**Рішення:** Низький пріоритет. Документувати як "не підтримується в v1".
Для production додати параметр `defrost.air_defrost_fan` (bool).

---

### B14: Defrost Phase transition — немає guard проти повторного входу

**Реалізація (defrost_module.cpp:130):**
```cpp
void DefrostModule::enter_phase(Phase p) {
    phase_ = p;
    phase_timer_ms_ = 0;
    ...
}
```

**Проблема:** Якщо enter_phase() викликається з тією ж фазою (наприклад, через
баг у логіці або race condition) — phase_timer_ms_ скидається в 0, state_set
публікується знову, ESP_LOGI логує "Phase → active" повторно. Для hot gas defrost
повторний вхід в ACTIVE може "зациклити" фазу назавжди (таймер скидається).

**Рішення:** Додати guard: `if (phase_ == p) return;` на початку enter_phase().

---

### B15: Defrost не перевіряє defrost_type при старті hot gas фаз

**Реалізація (defrost_module.cpp:344-352):**
```cpp
void DefrostModule::start_defrost(const char* reason) {
    if (defrost_type_ == 2) {
        enter_phase(Phase::STABILIZE);
    } else {
        enter_phase(Phase::ACTIVE);
    }
}
```

Це правильно для start. Але `finish_active_phase()`:
```cpp
void DefrostModule::finish_active_phase(const char* reason) {
    if (defrost_type_ == 2) {
        enter_phase(Phase::EQUALIZE);
    } else {
        enter_phase(Phase::DRIP);
    }
}
```

**Проблема:** Якщо `defrost_type_` **зміниться через WebUI під час активної відтайки**
(наприклад, з 2 на 0), `finish_active_phase` піде в DRIP замість EQUALIZE. Для
hot gas defrost пропуск EQUALIZE = **компресор запуститься під тиском → гідроудар**.

sync_settings() читає defrost.type кожен цикл — зміна застосується миттєво.

**Рішення:** Зберігати `active_defrost_type_` при start_defrost() і використовувати
його для всіх transition рішень до finish_defrost(). Не дозволяти зміну типу
під час активної відтайки.

---

## Пріоритезація

### 🔴 Критичні (можуть пошкодити обладнання)

| # | Проблема | Ризик | Зусилля |
|---|----------|-------|---------|
| **B15** | defrost_type зміна під час HG defrost → пропуск EQUALIZE | Гідроудар компресора | 15 хв |
| **B4** | Alarm не зупиняє компресор | Знос компресора при high temp | 1 год |
| **B2** | Protection не блокує нічого (lockout=false завжди) | Protection — декоративний | 2 год |
| **B6** | Cond fan різко OFF при defrost (без COd) | Тисковий удар | 1 год |

### 🟠 Важливі (неправильна поведінка)

| # | Проблема | Ризик | Зусилля |
|---|----------|-------|---------|
| **B1** | comp_on_time хибний при defrost | Неправильні таймери захисту | 1 год |
| **B7** | High temp blocked у FAD (а не тільки DFR/DRP) | Пропущений alarm | 30 хв |
| **B8** | 3 timeout alarm — тільки лог, не alarm | Оператор не побачить | 30 хв |
| **B9** | Demand defrost без мін. інтервалу | Зациклення defrost | 30 хв |
| **B11** | Thermostat рахує request time, не actual time | Хибний min_on/min_off | 1 год |
| **B14** | enter_phase без guard → можливий reset таймера | Зациклення фази | 5 хв |

### 🟡 Низький пріоритет

| # | Проблема | Ризик | Зусилля |
|---|----------|-------|---------|
| **B3** | 1-cycle delay при старті defrost | Мінімальний | документація |
| **B5** | Подвійний механізм OFF evap fan | Заплутаний код | рефакторинг |
| **B10** | comp_off_time = 999999 magic number | Крихкий код | 15 хв |
| **B12** | Stale temperature на 1 цикл | Мінімальний | не потребує фіксу |
| **B13** | Air defrost fan mode не підтримується | Feature gap | Phase 10+ |

---

## Рекомендований порядок виправлень

### Крок 1 — Quick safety fixes (1 година)

1. **B15:** Зберігати `active_defrost_type_` при `start_defrost()`:
```cpp
void DefrostModule::start_defrost(const char* reason) {
    active_type_ = defrost_type_;  // Зафіксувати на весь цикл
    ...
}
// Всі transition: використовувати active_type_ замість defrost_type_
```

2. **B14:** Guard в `enter_phase()`:
```cpp
void DefrostModule::enter_phase(Phase p) {
    if (phase_ == p) return;  // Захист від повторного входу
    ...
}
```

3. **B10:** Прибрати magic number:
```cpp
// В on_init(), після sync_settings():
comp_off_time_ms_ = min_off_ms_;  // Дозволити перший старт після startup_delay
```

### Крок 2 — Protection integration (2-3 години)

4. **B2 + B4:** Визначити alarm → action матрицю і реалізувати lockout:
```cpp
// protection_module.cpp:
// ERR1 → lockout (компресор не може працювати без температури)
// High Temp тривалий → lockout (система не охолоджує)
// Low Temp → lockout (переохолодження)
// ERR2 → info only
// Door → info only
bool lockout = sensor1_.active ||
               (high_temp_.active && high_temp_duration > LOCKOUT_THRESHOLD) ||
               low_temp_.active;
state_set("protection.lockout", lockout);
```

### Крок 3 — Thermostat fixes (1-2 години)

5. **B1:** Скидати state machine при defrost:
```cpp
if (defrost_active_ && !defrost_fad_) {
    if (state_ == State::COOLING) enter_state(State::IDLE);
    request_compressor(false);
    ...
    return;
}
```

6. **B11:** Трекати фактичний стан компресора:
```cpp
bool actual_comp = read_bool("equipment.compressor");
if (actual_comp) {
    actual_comp_on_ms_ += dt_ms;
    actual_comp_off_ms_ = 0;
} else {
    actual_comp_off_ms_ += dt_ms;
    actual_comp_on_ms_ = 0;
}
// Використовувати actual_comp_on/off для min_on/min_off перевірок
```

### Крок 4 — Defrost + Protection refinements (1-2 години)

7. **B6:** Cond fan delayed OFF при старті defrost
8. **B7:** High temp blocking тільки для DFR/DRP фаз
9. **B8:** Defrost timeout alarm → state key + Protection monitoring
10. **B9:** Min interval для demand defrost

---

## Changelog
- 2026-02-19 — Створено. 15 проблем бізнес-логіки виявлено.
  4 критичних (B15, B4, B2, B6), 6 важливих, 5 низькопріоритетних.
