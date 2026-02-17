# ModESP v4 — Стандарт маніфестів (Manifest Standard)

> Версія стандарту: 1.0
> Дата: 2026-02-16
>
> Маніфести — ЄДИНЕ ДЖЕРЕЛО ПРАВДИ про компоненти системи.
> Все генерується з них: UI, C++ headers, MQTT topics, display screens.
> Якщо чогось немає в маніфесті — цього не існує для системи.

---

## Огляд системи маніфестів

### Чотири типи маніфестів

| # | Тип | Файл | Хто створює | Навіщо |
|---|-----|------|-------------|--------|
| 1 | **Board** | `data/board.json` | Розробник PCB | Що є на платі (GPIO, шини) |
| 2 | **Driver** | `drivers/<name>/manifest.json` | Розробник драйвера | Що драйвер вміє, що потребує |
| 3 | **Module** | `modules/<name>/manifest.json` | Розробник модуля | Бізнес-логіка, UI, MQTT |
| 4 | **Bindings** | `data/bindings.json` | Інтегратор/оператор | Хто з ким з'єднаний |

### Як вони пов'язані

```
Board manifest            Driver manifest           Module manifest
"що є на платі"           "що драйвер вміє"         "що модуль хоче"

 gpio_outputs:             driver: relay              requires:
   relay_1 (GPIO14)  ←───  hardware_type:      ←───   role: compressor
   relay_2 (GPIO16)         gpio_output                type: actuator
                            category: actuator         drivers: [relay]

 onewire_buses:            driver: ds18b20            requires:
   ow_1 (GPIO15)     ←───  hardware_type:      ←───   role: chamber_temp
                            onewire_bus                type: sensor
                            category: sensor           drivers: [ds18b20, ntc]
                   ↑                            ↑
                   └────── Bindings manifest ───┘
                           "хто з ким з'єднаний"
                            hardware: relay_1
                            driver: relay
                            role: compressor
                            module: thermostat
```

### Потік даних

```
                    ┌─────────────┐
                    │ project.json │  ← які модулі активні
                    └──────┬──────┘
                           │
              ┌────────────┼────────────┐
              ▼            ▼            ▼
      Module manifests  Driver manifests  Board manifest
              │            │            │
              └────────────┼────────────┘
                           ▼
                  generate_ui.py
                  (валідація + генерація)
                           │
         ┌─────────┬───────┼────────┐
         ▼         ▼       ▼        ▼
      ui.json  state_    mqtt_    display_
               meta.h    topics.h screens.h
```

---

## 1. Board Manifest (`data/board.json`)

### Призначення
Описує конкретну PCB: які периферії розведені, на яких GPIO.
ESP32 парсить цей файл при boot через ConfigService.

### Обов'язкові поля

```json
{
  "manifest_version": 1,
  "board": "назва_плати",
  "version": "1.0.0",
  "description": "Опис плати (необов'язковий)"
}
```

### Секції периферій

Кожна секція — масив об'єктів з обов'язковим `id` та `gpio`:

| Секція | hardware_type | Для чого |
|--------|--------------|----------|
| `gpio_outputs` | `gpio_output` | Реле, клапани, LED |
| `gpio_inputs` | `gpio_input` | Кнопки, контакти дверей |
| `onewire_buses` | `onewire_bus` | DS18B20 |
| `adc_channels` | `adc_channel` | NTC, датчик тиску |
| `pwm_channels` | `pwm_channel` | Вентилятори |
| `i2c_buses` | `i2c_bus` | BME280, дисплеї |

### Правила
- `id` — унікальний в межах файлу, max 16 символів, `[a-z0-9_]`
- `gpio` — валідний для цільового чіпа
- `label` — необов'язковий, для UI замість id

### Повний приклад

```json
{
  "manifest_version": 1,
  "board": "cold_room_dev_v1",
  "version": "1.0.0",
  "description": "Dev board — холодильна камера, ESP32-WROOM",
  "gpio_outputs": [
    {"id": "relay_1", "gpio": 14, "active_high": true,  "label": "Реле 1"},
    {"id": "relay_2", "gpio": 16, "active_high": true,  "label": "Реле 2"}
  ],
  "onewire_buses": [
    {"id": "ow_1", "gpio": 15, "label": "OneWire шина 1"}
  ],
  "gpio_inputs": [
    {"id": "din_1", "gpio": 34, "pull": "up", "label": "Контакт дверей"}
  ],
  "adc_channels": [
    {"id": "adc_1", "gpio": 36, "atten": 11, "label": "ADC 1"}
  ]
}
```

---

## 2. Driver Manifest (`drivers/<name>/manifest.json`)

### Призначення
Описує можливості драйвера: який hardware потребує, що надає,
які налаштування має. Використовується ТІЛЬКИ генератором (не на ESP).

### ⚠️ ЗАРАЗ ВІДСУТНІ НА ДИСКУ
Driver manifests описані в документації, але файлів `drivers/ds18b20/manifest.json`
та `drivers/relay/manifest.json` поки немає. ЦЕ ТРЕБА СТВОРИТИ.

### Обов'язкові поля

```json
{
  "manifest_version": 1,
  "driver": "назва_драйвера",
  "description": "Опис",
  "category": "sensor | actuator | io",
  "hardware_type": "gpio_output | onewire_bus | adc_channel | ...",
  "provides": {
    "type": "float | int | bool | string",
    "unit": "°C",
    "range": [-55, 125]
  }
}
```

### Необов'язкові поля

```json
{
  "settings": [...],
  "requires_address": true,
  "multiple_per_bus": true,
  "dependencies": ["driver_name"]
}
```

### Поле `settings` — налаштування драйвера

Кожне налаштування — це параметр який можна змінити через UI або NVS:

```json
{
  "key": "read_interval_ms",
  "type": "int",
  "default": 1000,
  "min": 500,
  "max": 60000,
  "step": 100,
  "unit": "мс",
  "description": "Інтервал опитування",
  "persist": true
}
```

Правила для settings:
- `key` — `[a-z0-9_]`, max 24 символи
- `type` float/int з `access: readwrite` — ОБОВ'ЯЗКОВО мають `min`, `max`, `step`
- `persist: true` — зберігається в NVS між перезавантаженнями
- NVS namespace: `<hardware_id>.<key>` (наприклад `ow_1.read_interval_ms`)

### Поле `requires_address`

Деякі драйвери потребують адресу пристрою на шині:
- DS18B20 на OneWire — `"requires_address": true`
- Relay на GPIO — не потребує адреси

Якщо `requires_address: true`, в bindings.json ОБОВ'ЯЗКОВЕ поле `address`.

### Поле `multiple_per_bus`

Чи може бути кілька пристроїв цього драйвера на одній шині:
- DS18B20 — `true` (до 127 на одній OneWire шині)
- Relay — `false` (один GPIO = одне реле)

### Поле `dependencies`

Якщо драйвер залежить від іншого (рідко):
```json
"dependencies": ["onewire_master"]
```

### Приклади

**DS18B20:**
```json
{
  "manifest_version": 1,
  "driver": "ds18b20",
  "description": "Dallas DS18B20 цифровий датчик температури",
  "category": "sensor",
  "hardware_type": "onewire_bus",
  "requires_address": true,
  "multiple_per_bus": true,
  "provides": {"type": "float", "unit": "°C", "range": [-55, 125]},
  "settings": [
    {"key": "read_interval_ms", "type": "int",   "default": 1000, "min": 500, "max": 60000, "step": 100, "unit": "мс", "description": "Інтервал опитування"},
    {"key": "offset",           "type": "float", "default": 0.0,  "min": -5.0, "max": 5.0, "step": 0.1, "unit": "°C", "description": "Корекція показань"},
    {"key": "resolution",       "type": "int",   "default": 12,   "min": 9,    "max": 12,  "step": 1,   "unit": "біт", "description": "Роздільна здатність"}
  ]
}
```

**Relay:**
```json
{
  "manifest_version": 1,
  "driver": "relay",
  "description": "GPIO реле (on/off з захистом мін. часу)",
  "category": "actuator",
  "hardware_type": "gpio_output",
  "requires_address": false,
  "multiple_per_bus": false,
  "provides": {"type": "bool"},
  "settings": [
    {"key": "min_off_time_s", "type": "int", "default": 180, "min": 0, "max": 600, "step": 10, "unit": "с", "description": "Мін. час вимкнення (захист компресора)"},
    {"key": "min_on_time_s",  "type": "int", "default": 60,  "min": 0, "max": 600, "step": 10, "unit": "с", "description": "Мін. час увімкнення"}
  ]
}
```


### 2.1 Driver UI — інтерфейс налаштувань драйвера

Драйвери можуть мати ВЛАСНИЙ UI для налаштування, незалежно від модуля.
Це потрібно коли:
- Драйвер має параметри які налаштовує СЕРВІСНИЙ ІНЖЕНЕР (не оператор)
- Потрібна інтерактивна дія (сканування шини, калібрування)
- Налаштування специфічні для заліза, а не для бізнес-логіки

**Нове поле в Driver Manifest — `ui`:**

```json
{
  "driver": "ds18b20",
  "ui": {
    "page": "Налаштування датчиків",
    "page_id": "driver_settings",
    "access_level": "service",
    "cards": [
      {
        "title": "DS18B20: {{hardware_id}}",
        "instance_per_binding": true,
        "widgets": [
          {"setting": "read_interval_ms", "widget": "number_input"},
          {"setting": "offset",           "widget": "slider"},
          {"setting": "resolution",       "widget": "select"}
        ]
      }
    ]
  }
}
```

**Ключові відмінності від Module UI:**

| Аспект | Module UI | Driver UI |
|--------|-----------|-----------|
| Хто бачить | Оператор | Сервісний інженер |
| Що налаштовує | Бізнес-параметри (уставка) | Апаратні параметри (offset, interval) |
| Скільки екземплярів | Один на модуль | Один на кожен binding |
| Ключ widget | `key` → state key | `setting` → driver setting |
| `access_level` | `operator` (за замовч.) | `service` (захищена сторінка) |

**`instance_per_binding: true`** — генератор створює окрему картку для кожного
binding з цим драйвером. `{{hardware_id}}` замінюється на реальний id:
- "DS18B20: ow_1" (chamber_temp → thermostat)
- "DS18B20: ow_2" (evap_temp → defrost)

**`access_level`** — визначає видимість:
- `operator` — основний UI, бачать всі
- `service` — сторінка "Сервіс", захищена паролем або окремим URL
- `factory` — тільки при першому налаштуванні (provisioning)

**Приклад: NTC з вибором типу термістора (presets):**

```json
{
  "driver": "ntc",
  "ui": {
    "page": "Налаштування датчиків",
    "page_id": "driver_settings",
    "access_level": "service",
    "cards": [
      {
        "title": "NTC: {{hardware_id}} ({{role}})",
        "instance_per_binding": true,
        "widgets": [
          {"setting": "beta", "widget": "select", "presets": [
            {"label": "NTC 10K B3950 (стандарт)", "value": 3950},
            {"label": "NTC 10K B3435",            "value": 3435},
            {"label": "NTC 5K B3380",             "value": 3380},
            {"label": "Ручне введення",           "value": null}
          ]},
          {"setting": "r_nominal",        "widget": "number_input"},
          {"setting": "r_series",         "widget": "number_input"},
          {"setting": "offset",           "widget": "slider"},
          {"setting": "read_interval_ms", "widget": "number_input"}
        ]
      }
    ]
  }
}
```

`{{role}}` — підставляється роль з bindings: "chamber_temp", "evap_temp" тощо.

---

### 2.2 Discovery / Scan — інтерактивне виявлення пристроїв на шині

Деякі драйвери підтримують автоматичне виявлення пристроїв.
Це ІНТЕРАКТИВНА ДІЯ: користувач натискає "Сканувати", ESP32 сканує шину,
повертає знайдені пристрої.

**Нове поле в Driver Manifest — `discovery`:**

```json
{
  "driver": "ds18b20",
  "discovery": {
    "supported": true,
    "scan_endpoint": "/api/drivers/ds18b20/scan",
    "returns": [
      {"field": "address",     "type": "string", "description": "ROM адреса (64-bit)"},
      {"field": "temperature", "type": "float",  "description": "Поточне показання (для ідентифікації)"},
      {"field": "parasitic",   "type": "bool",   "description": "Паразитне живлення"}
    ],
    "ui": {
      "title": "Сканер OneWire шини",
      "description": "Натисніть 'Сканувати' для пошуку датчиків. По температурі можна визначити де фізично стоїть датчик.",
      "scan_button": "Сканувати",
      "result_table": true,
      "assign_action": true
    }
  }
}
```

**Як працює (сценарій сервісного інженера):**

```
1. Інженер відкриває "Сервіс" → "Сканер датчиків"
2. Вибирає шину: [ow_1 ▾]  (список з board.json)
3. Натискає [Сканувати]
4. ESP32 сканує OneWire шину, повертає:

   GET /api/drivers/ds18b20/scan?bus=ow_1

   {
     "bus": "ow_1",
     "devices": [
       {"address": "28:FF:A1:B2:C3:D4:E5:01", "temperature": -18.5, "parasitic": false},
       {"address": "28:FF:F2:G3:H4:I5:J6:02", "temperature":   4.2, "parasitic": false},
       {"address": "28:FF:X7:Y8:Z9:W0:V1:03", "temperature":  35.1, "parasitic": false}
     ]
   }

5. UI показує таблицю:
   ┌──────────────────────────────┬──────────┬───────────┐
   │ Адреса                       │ Темп.    │ Дія       │
   ├──────────────────────────────┼──────────┼───────────┤
   │ 28:FF:A1:B2:C3:D4:E5:01     │ -18.5°C  │ [Призначити ▾] │
   │ 28:FF:F2:G3:H4:I5:J6:02     │   4.2°C  │ [Призначити ▾] │
   │ 28:FF:X7:Y8:Z9:W0:V1:03     │  35.1°C  │ [Призначити ▾] │
   └──────────────────────────────┴──────────┴───────────┘

6. Інженер розуміє по температурі:
   -18.5°C → це камера          → призначає роль "chamber_temp"
    4.2°C  → це випарник         → призначає роль "evap_temp"
   35.1°C  → це конденсатор     → призначає роль "condenser_temp"

7. UI оновлює bindings.json з правильними адресами
```

**`assign_action: true`** — після сканування UI показує dropdown з
доступними ролями (з requires всіх модулів в project).

**Для I2C — аналогічно:**

```json
{
  "driver": "bme280",
  "discovery": {
    "supported": true,
    "scan_endpoint": "/api/drivers/i2c/scan",
    "address_range": ["0x76", "0x77"],
    "returns": [
      {"field": "address", "type": "string"},
      {"field": "chip_id", "type": "string", "description": "Ідентифікатор чіпа"}
    ]
  }
}
```

---

### 2.1 Driver UI — інтерфейс налаштувань драйвера

Драйвери можуть мати ВЛАСНИЙ UI для налаштування, незалежно від модуля.
Це потрібно коли:
- Драйвер має параметри які налаштовує СЕРВІСНИЙ ІНЖЕНЕР (не оператор)
- Потрібна інтерактивна дія (сканування шини, калібрування)
- Налаштування специфічні для заліза, а не для бізнес-логіки

**Нове поле в Driver Manifest — `ui`:**

```json
{
  "driver": "ds18b20",
  "ui": {
    "page": "Налаштування датчиків",
    "page_id": "driver_settings",
    "access_level": "service",
    "cards": [
      {
        "title": "DS18B20: {{hardware_id}}",
        "instance_per_binding": true,
        "widgets": [
          {"setting": "read_interval_ms", "widget": "number_input"},
          {"setting": "offset",           "widget": "slider"},
          {"setting": "resolution",       "widget": "select"}
        ]
      }
    ]
  }
}
```

**Ключові відмінності від Module UI:**

| Аспект | Module UI | Driver UI |
|--------|-----------|-----------|
| Хто бачить | Оператор | Сервісний інженер |
| Що налаштовує | Бізнес-параметри (уставка) | Апаратні параметри (offset, interval) |
| Скільки екземплярів | Один на модуль | Один на кожен binding |
| Ключ widget | `key` → state key | `setting` → driver setting |
| `access_level` | `operator` (за замовч.) | `service` (захищена сторінка) |

**`instance_per_binding: true`** — генератор створює окрему картку для кожного
binding з цим драйвером. `{{hardware_id}}` замінюється на реальний id:
- "DS18B20: ow_1" (chamber_temp → thermostat)
- "DS18B20: ow_2" (evap_temp → defrost)

**`access_level`** — визначає видимість:
- `operator` — основний UI, бачать всі
- `service` — сторінка "Сервіс", захищена паролем або окремим URL
- `factory` — тільки при першому налаштуванні (provisioning)

**Приклад: NTC з вибором типу термістора (presets):**

```json
{
  "driver": "ntc",
  "ui": {
    "page": "Налаштування датчиків",
    "page_id": "driver_settings",
    "access_level": "service",
    "cards": [
      {
        "title": "NTC: {{hardware_id}} ({{role}})",
        "instance_per_binding": true,
        "widgets": [
          {"setting": "beta", "widget": "select", "presets": [
            {"label": "NTC 10K B3950 (стандарт)", "value": 3950},
            {"label": "NTC 10K B3435",            "value": 3435},
            {"label": "NTC 5K B3380",             "value": 3380},
            {"label": "Ручне введення",           "value": null}
          ]},
          {"setting": "r_nominal",        "widget": "number_input"},
          {"setting": "r_series",         "widget": "number_input"},

### 3.1.1 Role Context — контекст ролі (діапазони, алярми, одиниці)

Один і той самий драйвер (DS18B20) може стояти в різних місцях з РІЗНИМИ
допустимими діапазонами:
- **Камера** (-35°C..0°C) — температура +5°C вже аварія
- **Випарник** (-40°C..+20°C) — температура +20°C нормальна при розморозці
- **Конденсатор** (+20°C..+70°C) — температура -5°C це несправність

Ці діапазони — НЕ властивість драйвера і НЕ властивість модуля.
Це властивість РОЛІ — тобто КОНКРЕТНОГО використання датчика.

**Нове поле в requires — `role_context`:**

```json
"requires": [
  {
    "role": "chamber_temp",
    "type": "sensor",
    "driver": ["ds18b20", "ntc"],
    "role_context": {
      "expected_range": [-35, 0],
      "alarm_low": -30,
      "alarm_high": -10,
      "label": "Температура камери",
      "color_zones": [
        {"to": -25, "color": "#3b82f6"},
        {"to": -15, "color": "#22c55e"},
        {"to":   0, "color": "#f59e0b"},
        {"to":  10, "color": "#ef4444"}
      ]
    }
  },
  {
    "role": "evap_temp",
    "type": "sensor",
    "driver": ["ds18b20", "ntc"],
    "role_context": {
      "expected_range": [-40, 20],
      "alarm_low": -45,
      "alarm_high": 25,
      "label": "Температура випарника"
    }
  },
  {
    "role": "condenser_temp",
    "type": "sensor",
    "driver": ["ds18b20", "ntc"],
    "optional": true,
    "role_context": {
      "expected_range": [20, 60],
      "alarm_low": 10,
      "alarm_high": 65,
      "label": "Температура конденсатора"
    }
  }
]
```

**Поля `role_context`:**

| Поле | Тип | Опис |
|------|-----|------|
| `expected_range` | [min, max] | Нормальний робочий діапазон для цієї ролі |
| `alarm_low` | float | Поріг нижнього алярму |
| `alarm_high` | float | Поріг верхнього алярму |
| `label` | string | Людська назва (для UI замість role id) |
| `color_zones` | array | Кольорова шкала для gauge widget |
| `unit` | string | Одиниці (якщо відрізняються від driver.provides.unit) |

**Як використовується:**

1. **Генератор** — бере `color_zones` з role_context для gauge widget
   (замість захардкоджених в UI секції)
2. **Alarm Module** — бере `alarm_low`/`alarm_high` як defaults для алармів
3. **Валідатор** — перевіряє що `expected_range` входить в `driver.provides.range`
   (DS18B20 дає -55..125, камера використовує -35..0 — ОК)
4. **Scan/Discovery** — показує expected_range в UI для допомоги при
   ідентифікації датчиків ("цей -18°C — схоже на камеру")

**Зв'язок role_context → driver settings_override в bindings:**

role_context описує ЩО ОЧІКУЄТЬСЯ від цієї ролі (семантика).
settings_override в bindings описує ЯК НАЛАШТУВАТИ конкретний hardware.

```
role_context.expected_range = [-35, 0]     ← "камера працює в цьому діапазоні"
settings_override.offset = -0.3            ← "цей конкретний датчик показує на 0.3° більше"
```



### 3.1.1 Role Context — контекст ролі (діапазони, алярми, одиниці)

Один і той самий драйвер (DS18B20) може стояти в різних місцях з РІЗНИМИ
допустимими діапазонами:
- **Камера** (-35°C..0°C) — температура +5°C вже аварія
- **Випарник** (-40°C..+20°C) — температура +20°C нормальна при розморозці
- **Конденсатор** (+20°C..+70°C) — температура -5°C це несправність

Ці діапазони — НЕ властивість драйвера і НЕ властивість модуля.
Це властивість РОЛІ — тобто КОНКРЕТНОГО використання датчика.

**Нове поле в requires — `role_context`:**

```json
"requires": [
  {
    "role": "chamber_temp",
    "type": "sensor",
    "driver": ["ds18b20", "ntc"],
    "role_context": {
      "expected_range": [-35, 0],
      "alarm_low": -30,
      "alarm_high": -10,
      "label": "Температура камери",
      "color_zones": [
        {"to": -25, "color": "#3b82f6"},
        {"to": -15, "color": "#22c55e"},
        {"to":   0, "color": "#f59e0b"},
        {"to":  10, "color": "#ef4444"}
      ]
    }
  },
  {
    "role": "evap_temp",
    "type": "sensor",
    "driver": ["ds18b20", "ntc"],
    "role_context": {
      "expected_range": [-40, 20],
      "alarm_low": -45,
      "alarm_high": 25,
      "label": "Температура випарника"
    }
  },
  {
    "role": "condenser_temp",
    "type": "sensor",
    "driver": ["ds18b20", "ntc"],
    "optional": true,
    "role_context": {
      "expected_range": [20, 60],
      "alarm_low": 10,
      "alarm_high": 65,
      "label": "Температура конденсатора"
    }
  }
]
```

**Поля `role_context`:**

| Поле | Тип | Опис |
|------|-----|------|
| `expected_range` | [min, max] | Нормальний робочий діапазон для цієї ролі |
| `alarm_low` | float | Поріг нижнього алярму |
| `alarm_high` | float | Поріг верхнього алярму |
| `label` | string | Людська назва (для UI замість role id) |
| `color_zones` | array | Кольорова шкала для gauge widget |
| `unit` | string | Одиниці (якщо відрізняються від driver.provides.unit) |

**Як використовується:**

1. **Генератор** — бере `color_zones` з role_context для gauge widget
   (замість захардкоджених в UI секції)
2. **Alarm Module** — бере `alarm_low`/`alarm_high` як defaults для алармів
3. **Валідатор** — перевіряє що `expected_range` входить в `driver.provides.range`
   (DS18B20 дає -55..125, камера використовує -35..0 — ОК)
4. **Scan/Discovery** — показує expected_range в UI для допомоги при
   ідентифікації датчиків ("цей -18°C — схоже на камеру")

**Зв'язок role_context → driver settings_override в bindings:**

role_context описує ЩО ОЧІКУЄТЬСЯ від цієї ролі (семантика).
settings_override в bindings описує ЯК НАЛАШТУВАТИ конкретний hardware.

```
role_context.expected_range = [-35, 0]     ← "камера працює в цьому діапазоні"
settings_override.offset = -0.3            ← "цей конкретний датчик показує на 0.3° більше"
```



### 3.1.1 Role Context — контекст ролі (діапазони, алярми, одиниці)

Один і той самий драйвер (DS18B20) може стояти в різних місцях з РІЗНИМИ
допустимими діапазонами:
- **Камера** (-35°C..0°C) — температура +5°C вже аварія
- **Випарник** (-40°C..+20°C) — температура +20°C нормальна при розморозці
- **Конденсатор** (+20°C..+70°C) — температура -5°C це несправність

Ці діапазони — НЕ властивість драйвера і НЕ властивість модуля.
Це властивість РОЛІ — тобто КОНКРЕТНОГО використання датчика.

**Нове поле в requires — `role_context`:**

```json
"requires": [
  {
    "role": "chamber_temp",
    "type": "sensor",
    "driver": ["ds18b20", "ntc"],
    "role_context": {
      "expected_range": [-35, 0],
      "alarm_low": -30,
      "alarm_high": -10,
      "label": "Температура камери",
      "color_zones": [
        {"to": -25, "color": "#3b82f6"},
        {"to": -15, "color": "#22c55e"},
        {"to":   0, "color": "#f59e0b"},
        {"to":  10, "color": "#ef4444"}
      ]
    }
  },
  {
    "role": "evap_temp",
    "type": "sensor",
    "driver": ["ds18b20", "ntc"],
    "role_context": {
      "expected_range": [-40, 20],
      "alarm_low": -45,
      "alarm_high": 25,
      "label": "Температура випарника"
    }
  },
  {
    "role": "condenser_temp",
    "type": "sensor",
    "driver": ["ds18b20", "ntc"],
    "optional": true,
    "role_context": {
      "expected_range": [20, 60],
      "alarm_low": 10,
      "alarm_high": 65,
      "label": "Температура конденсатора"
    }
  }
]
```

**Поля `role_context`:**

| Поле | Тип | Опис |
|------|-----|------|
| `expected_range` | [min, max] | Нормальний робочий діапазон для цієї ролі |
| `alarm_low` | float | Поріг нижнього алярму |
| `alarm_high` | float | Поріг верхнього алярму |
| `label` | string | Людська назва (для UI замість role id) |
| `color_zones` | array | Кольорова шкала для gauge widget |
| `unit` | string | Одиниці (якщо відрізняються від driver.provides.unit) |

**Як використовується:**

1. **Генератор** — бере `color_zones` з role_context для gauge widget
   (замість захардкоджених в UI секції)
2. **Alarm Module** — бере `alarm_low`/`alarm_high` як defaults для алармів
3. **Валідатор** — перевіряє що `expected_range` входить в `driver.provides.range`
   (DS18B20 дає -55..125, камера використовує -35..0 — ОК)
4. **Scan/Discovery** — показує expected_range в UI для допомоги при
   ідентифікації датчиків ("цей -18°C — схоже на камеру")

**Зв'язок role_context → driver settings_override в bindings:**

role_context описує ЩО ОЧІКУЄТЬСЯ від цієї ролі (семантика).
settings_override в bindings описує ЯК НАЛАШТУВАТИ конкретний hardware.

```
role_context.expected_range = [-35, 0]     ← "камера працює в цьому діапазоні"
settings_override.offset = -0.3            ← "цей конкретний датчик показує на 0.3° більше"
```


          {"setting": "offset",           "widget": "slider"},
          {"setting": "read_interval_ms", "widget": "number_input"}
        ]
      }
    ]
  }
}
```

`{{role}}` — підставляється роль з bindings: "chamber_temp", "evap_temp" тощо.

---

### 2.2 Discovery / Scan — інтерактивне виявлення пристроїв на шині

Деякі драйвери підтримують автоматичне виявлення пристроїв.
Це ІНТЕРАКТИВНА ДІЯ: користувач натискає "Сканувати", ESP32 сканує шину,
повертає знайдені пристрої.

**Нове поле в Driver Manifest — `discovery`:**

```json
{
  "driver": "ds18b20",
  "discovery": {
    "supported": true,
    "scan_endpoint": "/api/drivers/ds18b20/scan",
    "returns": [
      {"field": "address",     "type": "string", "description": "ROM адреса (64-bit)"},
      {"field": "temperature", "type": "float",  "description": "Поточне показання (для ідентифікації)"},
      {"field": "parasitic",   "type": "bool",   "description": "Паразитне живлення"}
    ],
    "ui": {
      "title": "Сканер OneWire шини",
      "description": "Натисніть 'Сканувати' для пошуку датчиків. По температурі можна визначити де фізично стоїть датчик.",
      "scan_button": "Сканувати",
      "result_table": true,
      "assign_action": true
    }
  }
}
```

**Як працює (сценарій сервісного інженера):**

```
1. Інженер відкриває "Сервіс" → "Сканер датчиків"
2. Вибирає шину: [ow_1 ▾]  (список з board.json)
3. Натискає [Сканувати]
4. ESP32 сканує OneWire шину, повертає:

   GET /api/drivers/ds18b20/scan?bus=ow_1

   {
     "bus": "ow_1",
     "devices": [
       {"address": "28:FF:A1:B2:C3:D4:E5:01", "temperature": -18.5, "parasitic": false},
       {"address": "28:FF:F2:G3:H4:I5:J6:02", "temperature":   4.2, "parasitic": false},
       {"address": "28:FF:X7:Y8:Z9:W0:V1:03", "temperature":  35.1, "parasitic": false}
     ]
   }

5. UI показує таблицю:
   ┌──────────────────────────────┬──────────┬───────────┐
   │ Адреса                       │ Темп.    │ Дія       │
   ├──────────────────────────────┼──────────┼───────────┤
   │ 28:FF:A1:B2:C3:D4:E5:01     │ -18.5°C  │ [Призначити ▾] │
   │ 28:FF:F2:G3:H4:I5:J6:02     │   4.2°C  │ [Призначити ▾] │
   │ 28:FF:X7:Y8:Z9:W0:V1:03     │  35.1°C  │ [Призначити ▾] │
   └──────────────────────────────┴──────────┴───────────┘

6. Інженер розуміє по температурі:
   -18.5°C → це камера          → призначає роль "chamber_temp"
    4.2°C  → це випарник         → призначає роль "evap_temp"
   35.1°C  → це конденсатор     → призначає роль "condenser_temp"

7. UI оновлює bindings.json з правильними адресами
```

**`assign_action: true`** — після сканування UI показує dropdown з
доступними ролями (з requires всіх модулів в project).

**Для I2C — аналогічно:**

```json
{
  "driver": "bme280",
  "discovery": {
    "supported": true,
    "scan_endpoint": "/api/drivers/i2c/scan",
    "address_range": ["0x76", "0x77"],
    "returns": [
      {"field": "address", "type": "string"},
      {"field": "chip_id", "type": "string", "description": "Ідентифікатор чіпа"}
    ]
  }
}
```

---

### 2.1 Driver UI — інтерфейс налаштувань драйвера

Драйвери можуть мати ВЛАСНИЙ UI для налаштування, незалежно від модуля.
Це потрібно коли:
- Драйвер має параметри які налаштовує СЕРВІСНИЙ ІНЖЕНЕР (не оператор)
- Потрібна інтерактивна дія (сканування шини, калібрування)
- Налаштування специфічні для заліза, а не для бізнес-логіки

**Нове поле в Driver Manifest — `ui`:**

```json
{
  "driver": "ds18b20",
  "ui": {
    "page": "Налаштування датчиків",
    "page_id": "driver_settings",
    "access_level": "service",
    "cards": [
      {
        "title": "DS18B20: {{hardware_id}}",
        "instance_per_binding": true,
        "widgets": [
          {"setting": "read_interval_ms", "widget": "number_input"},
          {"setting": "offset",           "widget": "slider"},
          {"setting": "resolution",       "widget": "select"}
        ]
      }
    ]
  }
}
```

**Ключові відмінності від Module UI:**

| Аспект | Module UI | Driver UI |
|--------|-----------|-----------|
| Хто бачить | Оператор | Сервісний інженер |
| Що налаштовує | Бізнес-параметри (уставка) | Апаратні параметри (offset, interval) |
| Скільки екземплярів | Один на модуль | Один на кожен binding |
| Ключ widget | `key` → state key | `setting` → driver setting |
| `access_level` | `operator` (за замовч.) | `service` (захищена сторінка) |

**`instance_per_binding: true`** — генератор створює окрему картку для кожного
binding з цим драйвером. `{{hardware_id}}` замінюється на реальний id:
- "DS18B20: ow_1" (chamber_temp → thermostat)
- "DS18B20: ow_2" (evap_temp → defrost)

**`access_level`** — визначає видимість:
- `operator` — основний UI, бачать всі
- `service` — сторінка "Сервіс", захищена паролем або окремим URL
- `factory` — тільки при першому налаштуванні (provisioning)

**Приклад: NTC з вибором типу термістора (presets):**

```json
{
  "driver": "ntc",
  "ui": {
    "page": "Налаштування датчиків",
    "page_id": "driver_settings",
    "access_level": "service",
    "cards": [
      {
        "title": "NTC: {{hardware_id}} ({{role}})",
        "instance_per_binding": true,
        "widgets": [
          {"setting": "beta", "widget": "select", "presets": [
            {"label": "NTC 10K B3950 (стандарт)", "value": 3950},
            {"label": "NTC 10K B3435",            "value": 3435},
            {"label": "NTC 5K B3380",             "value": 3380},
            {"label": "Ручне введення",           "value": null}
          ]},
          {"setting": "r_nominal",        "widget": "number_input"},
          {"setting": "r_series",         "widget": "number_input"},
          {"setting": "offset",           "widget": "slider"},
          {"setting": "read_interval_ms", "widget": "number_input"}
        ]
      }
    ]
  }
}
```

`{{role}}` — підставляється роль з bindings: "chamber_temp", "evap_temp" тощо.

---

### 2.2 Discovery / Scan — інтерактивне виявлення пристроїв на шині

Деякі драйвери підтримують автоматичне виявлення пристроїв.
Це ІНТЕРАКТИВНА ДІЯ: користувач натискає "Сканувати", ESP32 сканує шину,
повертає знайдені пристрої.

**Нове поле в Driver Manifest — `discovery`:**

```json
{
  "driver": "ds18b20",
  "discovery": {
    "supported": true,
    "scan_endpoint": "/api/drivers/ds18b20/scan",
    "returns": [
      {"field": "address",     "type": "string", "description": "ROM адреса (64-bit)"},
      {"field": "temperature", "type": "float",  "description": "Поточне показання (для ідентифікації)"},
      {"field": "parasitic",   "type": "bool",   "description": "Паразитне живлення"}
    ],
    "ui": {
      "title": "Сканер OneWire шини",
      "description": "Натисніть 'Сканувати' для пошуку датчиків. По температурі можна визначити де фізично стоїть датчик.",
      "scan_button": "Сканувати",
      "result_table": true,
      "assign_action": true
    }
  }
}
```

**Як працює (сценарій сервісного інженера):**

```
1. Інженер відкриває "Сервіс" → "Сканер датчиків"
2. Вибирає шину: [ow_1 ▾]  (список з board.json)
3. Натискає [Сканувати]
4. ESP32 сканує OneWire шину, повертає:

   GET /api/drivers/ds18b20/scan?bus=ow_1

   {
     "bus": "ow_1",
     "devices": [
       {"address": "28:FF:A1:B2:C3:D4:E5:01", "temperature": -18.5, "parasitic": false},
       {"address": "28:FF:F2:G3:H4:I5:J6:02", "temperature":   4.2, "parasitic": false},
       {"address": "28:FF:X7:Y8:Z9:W0:V1:03", "temperature":  35.1, "parasitic": false}
     ]
   }

5. UI показує таблицю:
   ┌──────────────────────────────┬──────────┬───────────┐
   │ Адреса                       │ Темп.    │ Дія       │
   ├──────────────────────────────┼──────────┼───────────┤
   │ 28:FF:A1:B2:C3:D4:E5:01     │ -18.5°C  │ [Призначити ▾] │
   │ 28:FF:F2:G3:H4:I5:J6:02     │   4.2°C  │ [Призначити ▾] │
   │ 28:FF:X7:Y8:Z9:W0:V1:03     │  35.1°C  │ [Призначити ▾] │
   └──────────────────────────────┴──────────┴───────────┘

6. Інженер розуміє по температурі:
   -18.5°C → це камера          → призначає роль "chamber_temp"
    4.2°C  → це випарник         → призначає роль "evap_temp"
   35.1°C  → це конденсатор     → призначає роль "condenser_temp"

7. UI оновлює bindings.json з правильними адресами
```

**`assign_action: true`** — після сканування UI показує dropdown з
доступними ролями (з requires всіх модулів в project).

**Для I2C — аналогічно:**

```json
{
  "driver": "bme280",
  "discovery": {
    "supported": true,
    "scan_endpoint": "/api/drivers/i2c/scan",
    "address_range": ["0x76", "0x77"],
    "returns": [
      {"field": "address", "type": "string"},
      {"field": "chip_id", "type": "string", "description": "Ідентифікатор чіпа"}
    ]
  }
}
```

---
---

## 3. Module Manifest (`modules/<n>/manifest.json`)

### Призначення
Описує бізнес-логіку модуля: які ролі (сенсори/актуатори) потребує,
які state keys створює, як виглядає UI, що публікувати по MQTT.
Це найскладніший і найважливіший маніфест.

### Обов'язкові секції

```json
{
  "manifest_version": 1,
  "module": "назва_модуля",
  "description": "Опис",
  "requires": [...],
  "state": {...}
}
```

### Необов'язкові секції

```json
{
  "inputs": {...},
  "ui": {...},
  "mqtt": {...},
  "display": {...},
  "dependencies": [...]
}
```

---

### 3.1 Секція `requires` — залежності від драйверів

Описує ЩО модуль потребує для роботи. Кожен require — це "роль":

```json
"requires": [
  {
    "role": "chamber_temp",
    "type": "sensor",
    "driver": "ds18b20",
    "optional": false,
    "description": "Датчик температури камери"
  },
  {
    "role": "compressor",
    "type": "actuator",
    "driver": "relay",
    "optional": false,
    "description": "Реле компресора"
  },
  {
    "role": "door_contact",
    "type": "sensor",
    "driver": "digital_input",
    "optional": true,
    "description": "Контакт дверей (необов'язковий)"
  }
]
```

Правила:
- `role` — унікальна в межах модуля, `[a-z0-9_]`
- `type` — `sensor` або `actuator`
- `driver` — назва драйвера з `drivers/` (або масив допустимих: `["ds18b20", "ntc"]`)
- `optional: true` — модуль працює і без цього, але з обмеженнями
- Bindings.json ПОВИНЕН мати binding для кожного non-optional require

**Масив допустимих драйверів:**
```json
{"role": "evap_temp", "type": "sensor", "driver": ["ds18b20", "ntc"]}
```
Це означає: "мені потрібен температурний сенсор, підходить DS18B20 або NTC".

---

### 3.2 Секція `state` — ключі SharedState

Описує ВСІ state keys які модуль створює та/або читає:

```json
"state": {
  "thermostat.temperature": {
    "type": "float",
    "unit": "°C",
    "access": "read",
    "description": "Поточна температура камери"
  },
  "thermostat.setpoint": {
    "type": "float",
    "unit": "°C",
    "access": "readwrite",
    "min": -35.0,
    "max": 0.0,
    "step": 0.5,
    "default": -18.0,
    "persist": true,
    "description": "Уставка температури"
  },
  "thermostat.state": {
    "type": "string",
    "access": "read",
    "enum": ["idle", "cooling", "safe_mode"],
    "description": "Стан модуля"
  }
}
```

**Правила для state keys:**

| Правило | Опис |
|---------|------|
| Формат ключа | `<module>.<key>`, `[a-z0-9_.]`, max 24 символи |
| `access: read` | Модуль пише, UI/MQTT тільки читає |
| `access: readwrite` | UI/MQTT можуть змінювати через POST /api/settings |
| readwrite float/int | ОБОВ'ЯЗКОВО: `min`, `max`, `step` |
| readwrite bool | Не потребує min/max |
| `enum` | Тільки для `type: string`, список допустимих значень |
| `persist: true` | Зберігається в NVS, відновлюється при boot |
| `default` | Значення при першому запуску (до persist) |

---

### 3.2a Секція `inputs` — зовнішні state keys які модуль читає

Декларує які ключі з SharedState, опубліковані **іншими** модулями,
цей модуль читає під час роботи. Це контракт залежностей на рівні даних.

**Навіщо:**
- Генератор валідує: чи існує джерело ключа? Чи тип сумісний?
- При видаленні модуля-джерела → warning "thermostat очікує defrost.active"
- Автоматична документація: граф залежностей між модулями
- C++ header: перелік input keys для compile-time перевірки

**Формат:**

```json
"inputs": {
  "defrost.active": {
    "type": "bool",
    "source_module": "defrost",
    "optional": true,
    "default_if_missing": false,
    "description": "Чи йде зараз розморозка"
  },
  "door.open": {
    "type": "bool",
    "source_module": "door",
    "optional": true,
    "default_if_missing": false,
    "description": "Чи відкриті двері камери"
  }
}
```

**Поля:**

| Поле | Обов'язкове | Опис |
|------|-------------|------|
| `type` | ✅ | Очікуваний тип (`float`, `int`, `bool`, `string`) |
| `source_module` | ✅ | Який модуль публікує цей ключ |
| `optional` | ❌ | Default: `true`. Якщо `false` — модуль не стартує без джерела |
| `default_if_missing` | ❌ | Значення якщо ключ відсутній в SharedState |
| `description` | ❌ | Для документації та UI |

**Правила:**

1. Ключ в `inputs` НЕ МОЖЕ бути одночасно в `state` цього ж модуля (конфлікт: хто пише?)
2. `source_module` повинен існувати в `project.json` АБО `optional: true`
3. `source_module` повинен мати цей ключ в своїй секції `state`
4. `type` повинен збігатися з типом в `state` модуля-джерела
5. Якщо `optional: false` і модуля-джерела немає — помилка при build
6. Якщо `optional: true` і модуля-джерела немає — warning при build

**Runtime поведінка:**

Модуль викликає `shared_state.get("defrost.active")`:
- Якщо ключ є → повертає значення
- Якщо ключа немає → повертає `nullopt`
- Модуль використовує `default_if_missing` як fallback

Модуль НЕ ПОВИНЕН падати якщо optional input відсутній.

**Різниця між `inputs` та `dependencies`:**

| | `dependencies` | `inputs` |
|---|---|---|
| Що описує | Залежність від модуля | Залежність від конкретного ключа |
| Рівень | "Мені потрібен thermostat" | "Мені потрібен defrost.active (bool)" |
| Валідація | Модуль є в project.json? | Ключ є в state модуля? Тип збігається? |
| Коли потрібен | Порядок ініціалізації | Граф даних, документація |

Обидва можуть використовуватись разом. `dependencies` гарантує порядок init,
`inputs` гарантує що дані будуть доступні.

**Приклад: Thermostat з inputs**

```json
{
  "module": "thermostat",
  "state": {
    "thermostat.temperature": {"type": "float", "access": "read", "unit": "°C"},
    "thermostat.setpoint": {"type": "float", "access": "readwrite", "min": -35, "max": 0, "step": 0.5},
    "thermostat.compressor": {"type": "bool", "access": "read"}
  },
  "inputs": {
    "defrost.active": {
      "type": "bool",
      "source_module": "defrost",
      "optional": true,
      "default_if_missing": false,
      "description": "Під час розморозки компресор не вмикається"
    },
    "door.open": {
      "type": "bool",
      "source_module": "door",
      "optional": true,
      "default_if_missing": false,
      "description": "При відкритих дверях можливий відкладений alarm"
    }
  }
}
```

**Приклад: Alarm модуль**

```json
{
  "module": "alarm",
  "state": {
    "alarm.active": {"type": "bool", "access": "read"},
    "alarm.code": {"type": "string", "access": "read", "enum": ["none", "high_temp", "low_temp", "sensor_fail"]},
    "alarm.acknowledge": {"type": "bool", "access": "readwrite"}
  },
  "inputs": {
    "thermostat.temperature": {
      "type": "float",
      "source_module": "thermostat",
      "optional": false,
      "description": "Температура для перевірки high/low alarm"
    },
    "thermostat.setpoint": {
      "type": "float",
      "source_module": "thermostat",
      "optional": false,
      "description": "Уставка для розрахунку порогів alarm"
    },
    "defrost.active": {
      "type": "bool",
      "source_module": "defrost",
      "optional": true,
      "default_if_missing": false,
      "description": "Під час розморозки alarm на температуру заглушується"
    }
  }
}
```

**Валідація генератором:**

```
[VALIDATE] Checking inputs...
✅ thermostat: input 'defrost.active' → defrost.state['defrost.active'] (bool=bool) OK
✅ thermostat: input 'door.open' → door not in project.json (optional, skipping)
✅ alarm: input 'thermostat.temperature' → thermostat.state['thermostat.temperature'] (float=float) OK
✅ alarm: input 'thermostat.setpoint' → thermostat.state['thermostat.setpoint'] (float=float) OK
⚠️ alarm: input 'defrost.active' → defrost not in project.json (optional, warning)
```

**Pattern: Mutual Exclusion між модулями**

Взаємовиключні модулі (наприклад thermostat і defrost) блокують один одного
через inputs — НЕ через блокування relay на рівні HAL.

```
defrost.active = true  → thermostat бачить через inputs → не вмикає компресор
thermostat.cooling = true → defrost бачить через inputs → не починає розморозку
```

Кожен модуль сам приймає рішення на основі domain knowledge.
HAL/DriverManager НЕ має логіки arbitration — це відповідальність модуля.

---

### 3.3 Секція `ui` — веб-інтерфейс

Описує як модуль виглядає в WebUI: сторінка, картки, віджети.

```json
"ui": {
  "page": "Холодильна камера",
  "page_id": "thermostat",
  "icon": "snowflake",
  "order": 1,
  "cards": [...]
}
```

| Поле | Обов'язкове | Опис |
|------|-------------|------|
| `page` | ✅ | Назва сторінки в UI (українською) |
| `page_id` | ✅ | URL slug, `[a-z0-9_]` |
| `icon` | ❌ | Іконка сторінки (snowflake, flame, gauge...) |
| `order` | ❌ | Порядок в навігації (1 = перша) |
| `cards` | ✅ | Масив карток з віджетами |

**Картка (card):**

```json
{
  "title": "Температура",
  "group": "settings",
  "collapsible": true,
  "widgets": [...]
}
```

- `title` — заголовок картки
- `group` — логічне групування (`"settings"` = згортається за замовчуванням)
- `collapsible` — чи можна згорнути
- `widgets` — масив віджетів (мінімум 1)

**Віджет (widget):**

```json
{
  "key": "thermostat.temperature",
  "widget": "gauge",
  "size": "large",
  "color_zones": [
    {"to": -22, "color": "#3b82f6"},
    {"to": -15, "color": "#22c55e"},
    {"to":  10, "color": "#ef4444"}
  ]
}
```

| Поле | Обов'язкове | Опис |
|------|-------------|------|
| `key` | ✅ | State key з секції `state` |
| `widget` | ✅ | Тип віджета (див. таблицю нижче) |
| `size` | ❌ | `small` / `medium` / `large` |
| інші | ❌ | Специфічні для типу віджета |

**Типи віджетів:**

| Widget | State type | Access | Опис | Специфічні поля |
|--------|-----------|--------|------|-----------------|
| `gauge` | float | read | Кругова шкала | `color_zones`, `min_value`, `max_value` |
| `slider` | float/int | readwrite | Повзунок | (бере min/max/step зі state) |
| `number_input` | float/int | readwrite | Числове поле з +/- | — |
| `toggle` | bool | readwrite | Перемикач on/off | `on_label`, `off_label` |
| `indicator` | bool | read | LED індикатор | `on_label`, `off_label`, `on_color`, `off_color` |
| `status_text` | string | read | Текстовий статус | `labels` (map enum→display) |
| `select` | string | readwrite | Випадаючий список | (бере enum зі state) |
| `chart` | float | read | Графік (Phase 7+) | `period`, `points` |

---

### 3.4 Секція `mqtt` — MQTT topics

```json
"mqtt": {
  "publish": [
    "thermostat.temperature",
    "thermostat.compressor",
    "thermostat.state"
  ],
  "subscribe": [
    "thermostat.setpoint",
    "thermostat.hysteresis"
  ]
}
```

- `publish` — state keys які публікуються при зміні
- `subscribe` — state keys які можна змінити по MQTT
- Формат topic: `modesp/<device_id>/<state_key>` (автоматично)
- subscribe ключі ПОВИННІ мати `access: readwrite` в state

---

### 3.5 Секція `display` — LCD/OLED екрани

```json
"display": {
  "main_value": {
    "key": "thermostat.temperature",
    "format": "%.1f°C"
  },
  "status_line": {
    "key": "thermostat.state"
  },
  "menu_items": [
    {"label": "Уставка",    "key": "thermostat.setpoint"},
    {"label": "Гістерезис", "key": "thermostat.hysteresis"}
  ]
}
```

- `main_value` — великі цифри на головному екрані
- `status_line` — рядок стану під головним значенням
- `menu_items` — пункти меню для зміни readwrite параметрів

---

### 3.6 Секція `dependencies` — залежності між модулями

Деякі модулі залежать від інших модулів (не від драйверів):

```json
"dependencies": [
  {
    "module": "thermostat",
    "required": true,
    "reason": "Defrost потребує стан компресора від thermostat"
  }
]
```

Приклади залежностей:
- **Defrost** → **Thermostat** (зупиняє компресор під час розморозки)
- **Alarm** → **Thermostat** (читає температуру для алармів)
- **Fan** → **Defrost** (вентилятор вимикається під час розморозки)

Правила:
- `required: true` — без цього модуля не стартує
- `required: false` — працює, але з обмеженнями
- Циклічні залежності ЗАБОРОНЕНІ (генератор валідує)

---

## 4. Bindings Manifest (`data/bindings.json`)

### Призначення
З'єднує всі рівні: Hardware → Driver → Role → Module.
Створюється інтегратором під конкретну інсталяцію.
В майбутньому — через UI конфігуратор.

### Schema

```json
{
  "manifest_version": 1,
  "bindings": [
    {
      "hardware": "relay_1",
      "driver": "relay",
      "role": "compressor",
      "module": "thermostat",
      "address": null,
      "settings_override": {}
    }
  ]
}
```

| Поле | Обов'язкове | Опис |
|------|-------------|------|
| `hardware` | ✅ | id з board.json |
| `driver` | ✅ | назва драйвера з drivers/ |
| `role` | ✅ | роль з module requires |
| `module` | ✅ | назва модуля з modules/ |
| `address` | * | Адреса на шині (* обов'язкове якщо driver.requires_address) |
| `settings_override` | ❌ | Перевизначення default settings драйвера |

### Приклад: повна конфігурація холодильної камери

```json
{
  "manifest_version": 1,
  "bindings": [
    {
      "hardware": "ow_1",
      "driver": "ds18b20",
      "role": "chamber_temp",
      "module": "thermostat",
      "address": "28:FF:A1:B2:C3:D4:E5:01",
      "settings_override": {
        "read_interval_ms": 2000,
        "offset": -0.3
      }
    },
    {
      "hardware": "relay_1",
      "driver": "relay",
      "role": "compressor",
      "module": "thermostat",
      "settings_override": {
        "min_off_time_s": 300
      }
    },
    {
      "hardware": "din_1",
      "driver": "digital_input",
      "role": "door_contact",
      "module": "thermostat",
      "settings_override": {
        "invert": true
      }
    }
  ]
}
```

### Правила валідації bindings

Генератор ПОВИНЕН перевірити:

1. `hardware` існує в board.json
2. `driver` існує в drivers/ та має manifest
3. `driver.hardware_type` збігається з типом hardware в board.json
4. `role` + `module` існують в module manifest requires
5. `driver` дозволений для цієї ролі (є в `requires[].driver`)
6. `address` присутня якщо `driver.requires_address == true`
7. Кожен non-optional require модуля має binding
8. Один hardware не може бути прив'язаний до двох ролей

---

## 5. Project Manifest (`project.json`)

### Призначення
Описує конкретну збірку проекту: які модулі активні, системні налаштування.
Це "верхній" рівень — точка входу для генератора.

### Schema

```json
{
  "manifest_version": 1,
  "project": "cold_room_v1",
  "version": "1.0.0",
  "description": "Холодильна камера — базова конфігурація",
  "modules": [
    "thermostat"
  ],
  "system": {
    "device_name": "ModESP",
    "pages": {
      "dashboard": true,
      "network": true,
      "system": true
    }
  }
}
```

---

## 6. Система залежностей та валідації

### Граф залежностей

При запуску генератора або при boot ESP32 будується граф:

```
project.json
  └── modules: ["thermostat", "defrost", "alarm"]
        │
        ├── thermostat/manifest.json
        │     requires: chamber_temp(ds18b20), compressor(relay)
        │     inputs: defrost.active(bool), door.open(bool)
        │
        ├── defrost/manifest.json
        │     requires: evap_temp(ds18b20|ntc), defrost_heater(relay)
        │     dependencies: [thermostat]
        │     inputs: thermostat.compressor(bool)
        │
        └── alarm/manifest.json
              requires: (none — читає state інших модулів)
              dependencies: [thermostat]
              inputs: thermostat.temperature(float), thermostat.setpoint(float), defrost.active(bool)

bindings.json → зв'язує requires з board.json через drivers
```

### Валідація (виконує generate_ui.py)

**Рівень 1: Синтаксис**
- [ ] Кожен manifest — валідний JSON
- [ ] manifest_version присутня і дорівнює 1
- [ ] Обов'язкові поля присутні

**Рівень 2: Цілісність посилань**
- [ ] Кожен module в project.json має папку в modules/
- [ ] Кожен driver в requires має папку в drivers/
- [ ] Кожен hardware в bindings існує в board.json
- [ ] Кожен state key відповідає формату `<module>.<key>`
- [ ] UI widget keys посилаються на існуючі state keys
- [ ] MQTT publish/subscribe keys існують у state

**Рівень 3: Залежності**
- [ ] Всі non-optional requires модулів мають bindings
- [ ] driver.hardware_type збігається з hardware type в board
- [ ] driver в binding дозволений для цієї ролі
- [ ] Немає циклічних module dependencies
- [ ] Порядок ініціалізації відповідає залежностям
- [ ] readwrite state keys з float/int мають min/max/step
- [ ] Кожен input key існує в state модуля `source_module`
- [ ] Тип input збігається з типом в state джерела
- [ ] `source_module` для non-optional inputs є в project.json

**Рівень 4: Попередження (не блокують)**
- [ ] optional requires без binding — warning
- [ ] optional inputs з відсутнім source_module — warning
- [ ] state key без UI widget — info
- [ ] state key без MQTT — info
- [ ] Driver settings з великим діапазоном — warning

### Вивід валідації

```
[VALIDATE] Loading project: cold_room_v1
[VALIDATE] Modules: thermostat, defrost, alarm
[VALIDATE] Checking manifests...
  ✅ thermostat/manifest.json — OK
  ✅ defrost/manifest.json — OK
  ✅ alarm/manifest.json — OK
[VALIDATE] Checking drivers...
  ✅ ds18b20/manifest.json — OK
  ✅ relay/manifest.json — OK
  ⚠️ digital_input/manifest.json — not found (used by: thermostat.door_contact [optional])
[VALIDATE] Checking bindings...
  ✅ ow_1 → ds18b20 → chamber_temp → thermostat
  ✅ relay_1 → relay → compressor → thermostat
  ⚠️ thermostat.door_contact — no binding (optional, skipping)
[VALIDATE] Checking dependencies...
  ✅ defrost depends on thermostat — present
  ✅ alarm depends on thermostat — present
  ✅ No circular dependencies
[VALIDATE] Checking inputs...
  ✅ thermostat: input 'defrost.active' (bool) → defrost.state OK
  ⚠️ thermostat: input 'door.open' → door not in project.json (optional, skipping)
  ✅ alarm: input 'thermostat.temperature' (float) → thermostat.state OK
  ✅ alarm: input 'thermostat.setpoint' (float) → thermostat.state OK
  ⚠️ alarm: input 'defrost.active' → defrost.state OK (optional)
[VALIDATE] Checking state keys...
  ✅ 5 read keys, 2 readwrite keys — all valid
[VALIDATE] Result: PASS (2 warnings)
```

---

## 7. Покрокова інструкція: створення нового модуля

### Приклад: Defrost Module (розморозка)

**Крок 1: Створити папку**
```
modules/defrost/
  ├── manifest.json
  └── (C++ код створюється окремо)
```

**Крок 2: Визначити requires (які драйвери потрібні)**

Питання: що defrost модуль фізично контролює?
- Потрібен датчик випарника → `evap_temp` sensor (ds18b20 або ntc)
- Потрібен нагрівач розморозки → `defrost_heater` actuator (relay)
- Необов'язково: вентилятор → `evap_fan` actuator (relay)

```json
"requires": [
  {"role": "evap_temp",       "type": "sensor",   "driver": ["ds18b20", "ntc"]},
  {"role": "defrost_heater",  "type": "actuator", "driver": "relay"},
  {"role": "evap_fan",        "type": "actuator", "driver": "relay", "optional": true}
]
```

**Крок 3: Визначити state keys**

Питання: які дані модуль створює і які параметри налаштовуються?

```json
"state": {
  "defrost.evap_temp":      {"type": "float",  "access": "read",      "unit": "°C", "description": "Температура випарника"},
  "defrost.active":         {"type": "bool",   "access": "read",      "description": "Розморозка активна"},
  "defrost.state":          {"type": "string", "access": "read",      "enum": ["idle", "defrost", "drip", "fan_delay"], "description": "Стан"},
  "defrost.interval_h":     {"type": "int",    "access": "readwrite", "min": 1, "max": 24, "step": 1, "default": 8, "persist": true, "unit": "год", "description": "Інтервал розморозок"},
  "defrost.duration_min":   {"type": "int",    "access": "readwrite", "min": 5, "max": 60, "step": 5, "default": 30, "persist": true, "unit": "хв", "description": "Макс. тривалість"},
  "defrost.end_temp":       {"type": "float",  "access": "readwrite", "min": 2, "max": 20, "step": 1, "default": 10, "persist": true, "unit": "°C", "description": "Температура завершення"},
  "defrost.drip_time_min":  {"type": "int",    "access": "readwrite", "min": 0, "max": 15, "step": 1, "default": 3, "persist": true, "unit": "хв", "description": "Час стоку води"}
}
```

**Крок 4: Визначити inputs (які чужі ключі модуль читає)**

```json
"inputs": {
  "thermostat.compressor": {
    "type": "bool",
    "source_module": "thermostat",
    "optional": false,
    "description": "Стан компресора — defrost вимикає його під час розморозки"
  }
}
```

**Крок 5: Визначити dependencies (залежності від інших модулів)**

```json
"dependencies": [
  {"module": "thermostat", "required": true, "reason": "Зупиняє компресор під час розморозки"}
]
```

**Крок 6: Додати UI**

```json
"ui": {
  "page": "Розморозка",
  "page_id": "defrost",
  "icon": "flame",
  "order": 2,
  "cards": [
    {
      "title": "Стан розморозки",
      "widgets": [
        {"key": "defrost.evap_temp", "widget": "gauge", "color_zones": [
          {"to": 0, "color": "#3b82f6"}, {"to": 10, "color": "#22c55e"}, {"to": 30, "color": "#ef4444"}
        ]},
        {"key": "defrost.active", "widget": "indicator", "on_label": "Розморозка", "off_label": "Норма"},
        {"key": "defrost.state", "widget": "status_text"}
      ]
    },
    {
      "title": "Налаштування",
      "group": "settings",
      "collapsible": true,
      "widgets": [
        {"key": "defrost.interval_h",   "widget": "number_input"},
        {"key": "defrost.duration_min",  "widget": "number_input"},
        {"key": "defrost.end_temp",      "widget": "number_input"},
        {"key": "defrost.drip_time_min", "widget": "number_input"}
      ]
    }
  ]
}
```

**Крок 6: MQTT та Display**

```json
"mqtt": {
  "publish": ["defrost.evap_temp", "defrost.active", "defrost.state"],
  "subscribe": ["defrost.interval_h", "defrost.duration_min", "defrost.end_temp"]
},
"display": {
  "main_value": {"key": "defrost.evap_temp", "format": "%.1f°C"},
  "status_line": {"key": "defrost.state"},
  "menu_items": [
    {"label": "Інтервал", "key": "defrost.interval_h"},
    {"label": "Тривалість", "key": "defrost.duration_min"},
    {"label": "Кінц.темп.", "key": "defrost.end_temp"}
  ]
}
```

**Крок 7: Додати модуль в project.json**

```json
"modules": ["thermostat", "defrost"]
```

**Крок 8: Додати bindings в bindings.json**

```json
{"hardware": "ow_2",    "driver": "ds18b20", "role": "evap_temp",      "module": "defrost", "address": "28:FF:..."},
{"hardware": "relay_2", "driver": "relay",   "role": "defrost_heater", "module": "defrost"},
{"hardware": "relay_3", "driver": "relay",   "role": "evap_fan",       "module": "defrost"}
```

**Крок 9: Запустити генератор**

```bash
python tools/generate_ui.py
```

Генератор:
1. Валідує всі маніфести
2. Генерує ui.json (тепер з двома сторінками)
3. Генерує C++ headers (state_meta.h, mqtt_topics.h, display_screens.h)

**Крок 10: Реалізувати C++ код модуля**

Після маніфесту — пишеш код, який використовує ролі:
```cpp
class DefrostModule : public BaseModule {
    void bind_drivers(DriverManager& dm) override {
        evap_sensor_  = dm.get_sensor("evap_temp");       // role з manifest
        heater_       = dm.get_actuator("defrost_heater"); // role з manifest
        fan_          = dm.get_actuator("evap_fan");       // optional
    }
};
```

---

## 8. Покрокова інструкція: створення нового драйвера

### Приклад: NTC Thermistor Driver

**Крок 1: Створити папку і маніфест**
```
drivers/ntc/
  ├── manifest.json
  └── (C++ код окремо)
```

**Крок 2: Заповнити маніфест**

Питання:
- Який hardware потрібен? → ADC канал → `"hardware_type": "adc_channel"`
- Що надає? → Температуру float в °C
- Чи потрібна адреса? → Ні (один ADC = один NTC)
- Які налаштування? → Beta, R_nominal, R_series, interval

```json
{
  "manifest_version": 1,
  "driver": "ntc",
  "description": "NTC термістор через ADC канал",
  "category": "sensor",
  "hardware_type": "adc_channel",
  "requires_address": false,
  "multiple_per_bus": false,
  "provides": {"type": "float", "unit": "°C", "range": [-40, 150]},
  "settings": [
    {"key": "beta",             "type": "int",   "default": 3950,  "min": 2000, "max": 5000,   "description": "B-коефіцієнт NTC"},
    {"key": "r_nominal",        "type": "int",   "default": 10000, "min": 1000, "max": 100000, "unit": "Ом", "description": "Номінальний опір при 25°C"},
    {"key": "r_series",         "type": "int",   "default": 10000, "min": 1000, "max": 100000, "unit": "Ом", "description": "Опір серійного резистора"},
    {"key": "read_interval_ms", "type": "int",   "default": 1000,  "min": 100,  "max": 60000,  "unit": "мс", "description": "Інтервал опитування"},
    {"key": "offset",           "type": "float", "default": 0.0,   "min": -5.0, "max": 5.0, "step": 0.1, "unit": "°C", "description": "Корекція"}
  ]
}
```

**Крок 3: Реалізувати C++ код**

Драйвер реалізує інтерфейс `ISensorDriver`:
```cpp
class NtcDriver : public ISensorDriver {
    float read() override;          // Повертає температуру
    const char* unit() override;    // "°C"
    bool is_valid() override;       // CRC/range check
};
```

**Крок 4: Тепер його можна використовувати в будь-якому модулі**

В thermostat manifest:
```json
{"role": "chamber_temp", "type": "sensor", "driver": ["ds18b20", "ntc"]}
```

В bindings.json:
```json
{"hardware": "adc_1", "driver": "ntc", "role": "chamber_temp", "module": "thermostat"}
```

---

## 9. TODO: Що потрібно створити

### Файли які відсутні і потрібно створити:

| Файл | Статус | Пріоритет |
|------|--------|-----------|
| `drivers/ds18b20/manifest.json` | ❌ Відсутній | ВИСОКИЙ |
| `drivers/relay/manifest.json` | ❌ Відсутній | ВИСОКИЙ |
| `drivers/ntc/manifest.json` | ❌ Відсутній | СЕРЕДНІЙ |
| `drivers/digital_input/manifest.json` | ❌ Відсутній | СЕРЕДНІЙ |
| `drivers/pwm_fan/manifest.json` | ❌ Відсутній | НИЗЬКИЙ |
| `manifest_version` в board.json | ❌ Відсутній | ВИСОКИЙ |
| `manifest_version` в bindings.json | ❌ Відсутній | ВИСОКИЙ |
| `manifest_version` в project.json | ❌ Відсутній | ВИСОКИЙ |
| `manifest_version` в thermostat/manifest.json | ❌ Відсутній | ВИСОКИЙ |
| Валідація в generate_ui.py | ❌ Часткова | ВИСОКИЙ |

### Зміни в generate_ui.py:

1. Читати driver manifests при генерації
2. Валідувати bindings проти driver manifests
3. Генерувати driver settings UI (для Phase 7 Svelte)
4. Виводити validation report з emoji (✅/⚠️/❌)

---

## Changelog
- v1.2 (2026-02-17) — Додано: `operational_range` в bindings (normal/warning/alarm),
  `recommended_range` в module requires, `label` в bindings,
  розширена валідація (правила 9-13). Файл очищено від дублікатів (416KB→70KB).
- v1.1 (2026-02-16) — Додано: Driver UI (секція 2.1), Discovery/Scan (секція 2.2).
- v1.0 (2026-02-16) — Створено повний стандарт маніфестів.
