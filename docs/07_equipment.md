# ModESP v4 — Equipment Manager & Protection

## Equipment Manager (equipment_module)

### Архітектура

Equipment Manager (EM) — єдиний модуль з прямим доступом до HAL drivers (ISensorDriver,
IActuatorDriver). Бізнес-модулі (Thermostat, Defrost, Protection) спілкуються з обладнанням
виключно через SharedState.

- **Priority:** CRITICAL (0) — ініціалізується першим, оновлюється першим
- **Bind:** `bind_drivers(DriverManager&)` — знаходить drivers за role name з bindings.json
- **Обов'язкові drivers:** `air_temp` (sensor), `compressor` (actuator)
- **Опціональні drivers:** evap_temp, condenser_temp, heater, evap_fan, cond_fan, hg_valve,
  door_contact, night_input

### Цикл on_update (кожна ітерація main loop)

```
1. read_sensors()      — читає сенсори (з EMA фільтром) → SharedState
2. read_requests()     — читає req.* від бізнес-модулів з SharedState
3. apply_arbitration() — арбітраж + інтерлоки → визначає бажані outputs
4. apply_outputs()     — застосовує outputs до relay (в тому самому циклі!)
5. publish_state()     — публікує ФАКТИЧНИЙ стан актуаторів (get_state())
```

Порядок 3-4 критичний: arbitration визначає outputs, apply одразу їх застосовує.
Попередній порядок (apply першим) створював осциляцію через однотактову затримку.

### Читання сенсорів

- **EMA (Exponential Moving Average)** фільтр з коефіцієнтом `equipment.filter_coeff` (0-10)
  - alpha = 1 / (coeff + 1); coeff=0 означає фільтр вимкнено (alpha=1.0)
- **Оптимістична ініціалізація:** sensor1_ok = sensor2_ok = true при старті
  - DS18B20 потребує ~750ms на першу конверсію, перші read() фейляться
  - Запобігає хибному ERR1/ERR2 від Protection до першого успішного зчитування
- **Здоров'я датчика:** `is_healthy()` (consecutive_errors tracking у драйвері)

### Арбітраж запитів

Кожен цикл EM читає requests від трьох бізнес-модулів і визначає фінальний стан виходів:

| Пріоритет | Джерело | Умова | Дія |
|-----------|---------|-------|-----|
| 1 (найвищий) | Protection | `protection.lockout = true` | Все OFF |
| 2 | Defrost | `defrost.active = true` | defrost.req.* визначають outputs |
| 3 (найнижчий) | Thermostat | нормальний режим | thermostat.req.* визначають outputs |

**SharedState keys, що читаються (requests):**

| Key | Джерело | Опис |
|-----|---------|------|
| `thermostat.req.compressor` | Thermostat | Запит на компресор |
| `thermostat.req.evap_fan` | Thermostat | Запит на вент. випарника |
| `thermostat.req.cond_fan` | Thermostat | Запит на вент. конденсатора |
| `defrost.active` | Defrost | Defrost активний (пріоритетний режим) |
| `defrost.req.compressor` | Defrost | Запит на компресор (FAD фаза) |
| `defrost.req.heater` | Defrost | Запит на тен відтайки |
| `defrost.req.evap_fan` | Defrost | Запит на вент. випарника |
| `defrost.req.cond_fan` | Defrost | Запит на вент. конденсатора |
| `defrost.req.hg_valve` | Defrost | Запит на клапан гарячого газу |
| `protection.lockout` | Protection | Аварійна зупинка (зарезервовано) |

### Compressor anti-short-cycle (output-level)

Захищає компресор незалежно від джерела запиту (thermostat або defrost).
Працює на фактичному стані реле, а не на запитах бізнес-модулів.

| Константа | Значення | Опис |
|-----------|----------|------|
| `COMP_MIN_OFF_MS` | 180000 (3 хв) | Мінімальний час компресор OFF |
| `COMP_MIN_ON_MS` | 120000 (2 хв) | Мінімальний час компресор ON |

- Запит ON блокується, якщо компресор був OFF менше 3 хвилин
- Запит OFF блокується (тримає ON), якщо компресор був ON менше 2 хвилин
- Доповнює (не замінює) таймери thermostat (ті працюють для state machine логіки)
- Таймер `comp_since_ms_` скидається при зміні фактичного стану реле

### Інтерлоки (hardcoded, неможливо обійти)

Виконуються останніми в apply_arbitration() — мають найвищий пріоритет після lockout:

1. **Тен + компресор НІКОЛИ одночасно** — якщо обидва ON, компресор вимикається
2. **Тен + клапан ГГ НІКОЛИ одночасно** — якщо обидва ON, клапан ГГ вимикається

### Safe Mode

При отриманні повідомлення `SYSTEM_SAFE_MODE` від MessageBus:
- Всі outputs скидаються в OFF (`out_ = {}`)
- Негайно застосовується до relay
- Публікується фактичний стан

### equipment.has_* state keys

При ініціалізації EM публікує наявність опціонального обладнання. Ці keys використовуються
для runtime UI visibility (visible_when) та per-option disabled (requires_state) у WebUI.

| State key | Тип | Опис | Визначається |
|-----------|-----|------|--------------|
| `equipment.has_heater` | bool | Тен відтайки доступний | `heater_ != nullptr` |
| `equipment.has_hg_valve` | bool | Клапан гарячого газу доступний | `hg_valve_ != nullptr` |
| `equipment.has_cond_fan` | bool | Вент. конденсатора доступний | `cond_fan_ != nullptr` |
| `equipment.has_door_contact` | bool | Контакт дверей доступний | `door_sensor_ != nullptr` |
| `equipment.has_evap_temp` | bool | Датчик випарника доступний | `sensor_evap_ != nullptr` |
| `equipment.has_cond_temp` | bool | Датчик конденсатора доступний | `sensor_cond_ != nullptr` |

Runtime ланцюг: Bindings page -> Save -> Restart -> equipment.has_* = true -> option enabled / card visible.

### Hardware roles (requires section маніфесту)

| Role | Type | Driver | Optional | Опис |
|------|------|--------|----------|------|
| `air_temp` | sensor | ds18b20 | **Ні** | Температура камери |
| `compressor` | actuator | relay | **Ні** | Компресор |
| `evap_temp` | sensor | ds18b20 | Так | Температура випарника |
| `condenser_temp` | sensor | ds18b20 | Так | Температура конденсатора |
| `heater` | actuator | relay | Так | Тен відтайки |
| `evap_fan` | actuator | relay | Так | Вентилятор випарника |
| `cond_fan` | actuator | relay | Так | Вентилятор конденсатора |
| `hg_valve` | actuator | relay | Так | Клапан гарячого газу |
| `door_contact` | sensor | digital_input | Так | Контакт дверей |
| `night_input` | sensor | digital_input | Так | Вхід нічного режиму |

### State keys (повна таблиця)

| Key | Type | Access | Unit | Persist | Опис |
|-----|------|--------|------|---------|------|
| `equipment.air_temp` | float | read | C | - | Температура камери |
| `equipment.evap_temp` | float | read | C | - | Температура випарника |
| `equipment.cond_temp` | float | read | C | - | Температура конденсатора |
| `equipment.sensor1_ok` | bool | read | - | - | Датчик камери справний |
| `equipment.sensor2_ok` | bool | read | - | - | Датчик випарника справний |
| `equipment.compressor` | bool | read | - | - | Фактичний стан компресора |
| `equipment.heater` | bool | read | - | - | Фактичний стан тена |
| `equipment.evap_fan` | bool | read | - | - | Фактичний стан вент. випарника |
| `equipment.cond_fan` | bool | read | - | - | Фактичний стан вент. конденсатора |
| `equipment.hg_valve` | bool | read | - | - | Фактичний стан клапана ГГ |
| `equipment.door_open` | bool | read | - | - | Стан дверей |
| `equipment.night_input` | bool | read | - | - | Дискретний вхід нічного режиму |
| `equipment.has_heater` | bool | read | - | - | Тен доступний (runtime) |
| `equipment.has_hg_valve` | bool | read | - | - | Клапан ГГ доступний (runtime) |
| `equipment.has_cond_fan` | bool | read | - | - | Вент. конденсатора доступний |
| `equipment.has_door_contact` | bool | read | - | - | Контакт дверей доступний |
| `equipment.has_evap_temp` | bool | read | - | - | Датчик випарника доступний |
| `equipment.has_cond_temp` | bool | read | - | - | Датчик конденсатора доступний |
| `equipment.filter_coeff` | int | readwrite | - | Так | Цифровий фільтр (0-10, default 4) |

### MQTT

**Publish:** equipment.air_temp, equipment.evap_temp, equipment.cond_temp,
equipment.compressor, equipment.heater, equipment.sensor1_ok

**Subscribe:** немає (EM не приймає команд ззовні)

---

## Protection Module (protection_module)

### Архітектура

Protection module моніторить 5 типів аварій і публікує стан через SharedState.
Не зупиняє обладнання напряму — `protection.lockout` зарезервований (завжди false).

- **Priority:** HIGH (1) — оновлюється ПІСЛЯ Equipment (0), ДО Thermostat (2)
- **Inputs:** читає equipment.air_temp, equipment.sensor1_ok, equipment.sensor2_ok,
  equipment.door_open, defrost.active, defrost.phase з SharedState
- **Features:** `basic_protection` (always_active), `door_protection` (requires door_contact role)

### 5 незалежних моніторів аварій

Кожен монітор має структуру AlarmMonitor: `active` (аварія зараз), `pending` (в затримці),
`pending_ms` (час в pending стані).

| Монітор | Код аварії | Тип затримки | Параметр затримки | Блокування defrost |
|---------|------------|--------------|-------------------|--------------------|
| High Temp (HAL) | `high_temp` | delayed | `high_alarm_delay` (хв) | Так (heating-фази + post-defrost) |
| Low Temp (LAL) | `low_temp` | delayed | `low_alarm_delay` (хв) | Ні |
| Sensor1 (ERR1) | `err1` | instant | - | Ні |
| Sensor2 (ERR2) | `err2` | instant | - | Ні |
| Door | `door` | delayed | `door_delay` (хв) | Ні |

### Алгоритм delayed alarm (High Temp, Low Temp, Door)

```
if (condition exceeded) {
    pending = true
    pending_ms += dt_ms
    if (pending_ms >= delay_ms) → active = true
} else {
    pending = false, pending_ms = 0
    if (active && !manual_reset) → active = false  // auto-clear
}
```

### Алгоритм instant alarm (Sensor1, Sensor2)

```
if (!sensor_ok && !active)  → active = true    // аварія негайно
if (sensor_ok && active && !manual_reset) → active = false  // auto-clear
```

### Defrost blocking (High Temp alarm)

High Temp alarm блокується у два етапи:

1. **Під час heating-фаз defrost** (stabilize, valve_open, active, equalize):
   - pending скидається, нові аварії не ставляться в чергу
   - Якщо аварія вже active — вона залишається
2. **Post-defrost suppression** (після завершення defrost):
   - Таймер `post_defrost_delay` (хвилини, default 30)
   - Протягом цього часу HAL alarm також блокується
   - Дозволяє температурі стабілізуватися після відтайки

Визначення heating-фази: EM читає `defrost.phase` і перевіряє чи це одна з:
"active", "stabilize", "valve_open", "equalize".

### Auto-clear vs Manual reset

- **Auto-clear** (manual_reset = false, default): аварія знімається автоматично при поверненні
  умови в норму (температура в діапазоні, датчик відновився, двері закрились)
- **Manual reset** (manual_reset = true): аварія залишається active навіть коли умова зникла.
  Потрібна команда `protection.reset_alarms = true` через WebUI або API

Команда reset_alarms скидає ВСІ активні аварії одночасно та повертає trigger в false.

### Пріоритет alarm_code

Коли кілька аварій активні одночасно, `protection.alarm_code` показує найвищу за пріоритетом:

1. `err1` — обрив датчика камери (найвищий пріоритет)
2. `high_temp` — перевищення верхньої межі
3. `low_temp` — перевищення нижньої межі
4. `err2` — обрив датчика випарника
5. `door` — двері відкриті занадто довго
6. `none` — аварій немає

### State keys (повна таблиця)

**Read-only (статус):**

| Key | Type | Опис |
|-----|------|------|
| `protection.lockout` | bool | Аварійна зупинка (зарезервовано, завжди false) |
| `protection.alarm_active` | bool | Є хоча б одна активна аварія |
| `protection.alarm_code` | string | Код найвищої аварії (err1/high_temp/low_temp/err2/door/none) |
| `protection.high_temp_alarm` | bool | Аварія верхньої температури |
| `protection.low_temp_alarm` | bool | Аварія нижньої температури |
| `protection.sensor1_alarm` | bool | Обрив датчика камери |
| `protection.sensor2_alarm` | bool | Обрив датчика випарника |
| `protection.door_alarm` | bool | Двері відкриті занадто довго |

**Readwrite (налаштування, persist в NVS):**

| Key | Type | Unit | Min | Max | Step | Default | Опис |
|-----|------|------|-----|-----|------|---------|------|
| `protection.high_limit` | float | C | -50.0 | 99.0 | 0.5 | 12.0 | Верхня межа температури |
| `protection.low_limit` | float | C | -99.0 | 50.0 | 0.5 | -35.0 | Нижня межа температури |
| `protection.high_alarm_delay` | int | хв | 0 | 120 | 1 | 30 | Затримка аварії високої T |
| `protection.low_alarm_delay` | int | хв | 0 | 120 | 1 | 30 | Затримка аварії низької T |
| `protection.door_delay` | int | хв | 0 | 60 | 1 | 5 | Затримка аварії дверей |
| `protection.manual_reset` | bool | - | - | - | - | false | Ручне скидання (false=auto) |
| `protection.post_defrost_delay` | int | хв | 0 | 120 | 5 | 30 | Suppress HAL alarm після defrost |

**Readwrite (команда):**

| Key | Type | Опис |
|-----|------|------|
| `protection.reset_alarms` | bool | Write-only trigger скидання всіх аварій |

### Features

| Feature | Always active | Requires roles | Controls settings |
|---------|---------------|----------------|-------------------|
| `basic_protection` | Так | - | high_limit, low_limit, high_alarm_delay, low_alarm_delay, manual_reset, post_defrost_delay |
| `door_protection` | Ні | door_contact | door_delay |

door_delay widget в UI має `visible_when: {"key": "equipment.has_door_contact", "eq": true}` —
показується тільки коли контакт дверей прив'язаний в bindings.

### MQTT

**Publish:** protection.lockout, protection.alarm_active, protection.alarm_code,
protection.high_temp_alarm, protection.low_temp_alarm, protection.sensor1_alarm,
protection.sensor2_alarm, protection.door_alarm

**Subscribe:** protection.high_limit, protection.low_limit, protection.high_alarm_delay,
protection.low_alarm_delay, protection.door_delay, protection.manual_reset,
protection.post_defrost_delay, protection.reset_alarms

---

## Порядок виконання в main loop

```
Equipment (priority 0)  → читає сенсори, арбітраж, публікує стан
Protection (priority 1) → моніторить аварії, публікує alarm state
Thermostat (priority 2) → регулювання, публікує req.compressor/fan
Defrost (priority 2)    → цикл відтайки, публікує req.heater/valve
```

Equipment завжди оновлюється першим, бо Protection та Thermostat залежать від
свіжих значень equipment.air_temp та equipment.sensor1_ok. Protection оновлюється
до Thermostat, бо Thermostat перевіряє protection.lockout.

## Changelog

- 2026-02-25 — Створено: Equipment Manager + Protection документація
