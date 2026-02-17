# ModESP v4 — Manifest Specification

## Мета документу

Формальний опис формату маніфестів для всіх типів компонентів ModESP v4.
Маніфести — єдине джерело правди про можливості компонента. На їх основі:
- Python-генератор (`tools/generate_ui.py`) будує WebUI та C++ метадані
- HAL/DriverManager валідує bindings при boot
- Оператор через WebUI бачить доступні опції

## Типи маніфестів

| Файл | Розташування | Відповідальність |
|------|-------------|-----------------|
| Board manifest | `data/board.json` | Фізичне залізо на PCB: GPIO → логічне ім'я |
| Driver manifest | `drivers/<name>/manifest.json` | Можливості драйвера: що потребує, що дає, налаштування |
| Module manifest | `modules/<name>/manifest.json` | Бізнес-логіка: які ролі потребує, state, UI, mqtt |

Зв'язок між ними:

```
Board manifest          Driver manifest          Module manifest
(що є на платі)         (що драйвер вміє)        (що модуль хоче)

 gpio_output:            driver: relay             requires:
   relay_1 (GPIO14)  ←── hardware_type:        ←── role: compressor
   relay_2 (GPIO16)      gpio_output                type: actuator
   relay_3 (GPIO5)       category: actuator         drivers: [relay]

 onewire_bus:            driver: ds18b20          requires:
   ow_1 (GPIO15)     ←── hardware_type:        ←── role: chamber_temp
   ow_2 (GPIO4)          onewire_bus                type: sensor
                         category: sensor           drivers: [ds18b20, ntc]
```

`bindings.json` — рантайм-клей: `relay_1 → relay driver → compressor → thermostat`

---

## 1. Board Manifest (`data/board.json`)

Описує конкретну PCB — які периферії розведені і на яких GPIO.
Може створюватися вручну або генеруватися тулзою під конкретну плату.
ESP парсить цей файл при boot через ConfigService.

### Schema

```json
{
  "board": "string",
  "version": "semver",
  "description": "string, optional",

  "gpio_outputs": [
    { "id": "string", "gpio": "int", "active_high": "bool=true", "label": "string?" }
  ],
  "onewire_buses": [
    { "id": "string", "gpio": "int", "label": "string?" }
  ],
  "adc_channels": [
    { "id": "string", "gpio": "int", "atten": "int=11", "label": "string?" }
  ],
  "pwm_channels": [
    { "id": "string", "gpio": "int", "frequency": "int=25000", "label": "string?" }
  ],
  "i2c_buses": [
    { "id": "string", "sda": "int", "scl": "int", "speed_hz": "int=100000", "label": "string?" }
  ],
  "gpio_inputs": [
    { "id": "string", "gpio": "int", "pull": "'up'|'down'|'none'", "label": "string?" }
  ]
}
```

### Приклад: холодильна камера dev board

```json
{
  "board": "cold_room_dev_v1",
  "version": "1.0.0",
  "description": "Плата розробки — холодильна камера, ESP32-WROOM",
  "gpio_outputs": [
    {"id": "relay_1", "gpio": 14, "active_high": true,  "label": "Реле 1"},
    {"id": "relay_2", "gpio": 16, "active_high": true,  "label": "Реле 2"},
    {"id": "relay_3", "gpio": 5,  "active_high": true,  "label": "Реле 3"},
    {"id": "relay_4", "gpio": 17, "active_high": false, "label": "Реле 4 (інв.)"}
  ],
  "onewire_buses": [
    {"id": "ow_1", "gpio": 15, "label": "OneWire шина 1"},
    {"id": "ow_2", "gpio": 4,  "label": "OneWire шина 2"}
  ],
  "adc_channels": [
    {"id": "adc_1", "gpio": 36, "atten": 11, "label": "ADC 1"},
    {"id": "adc_2", "gpio": 39, "atten": 11, "label": "ADC 2"}
  ],
  "gpio_inputs": [
    {"id": "din_1", "gpio": 34, "pull": "up", "label": "Дискретний вхід 1"},
    {"id": "din_2", "gpio": 35, "pull": "up", "label": "Дискретний вхід 2"}
  ]
}
```

### Правила

- **id** — унікальний в межах файлу, max 16 символів, `[a-z0-9_]`
- **gpio** — валідний для цільового чіпа
- **label** — для відображення в UI замість id
- Одна секція на кожен hardware_type

---

## 2. Driver Manifest (`drivers/<name>/manifest.json`)

Описує що драйвер вміє, який hardware потребує, які налаштування має.
Не потрапляє на ESP — використовується тільки Python-генератором.

### Schema

```json
{
  "driver": "string — збігається з назвою папки",
  "description": "string",
  "category": "'sensor' | 'actuator' | 'io'",
  "hardware_type": "string — ключ секції з board.json",
  "provides": {
    "type": "'float' | 'int' | 'bool' | 'string'",
    "unit": "string?",
    "range": [min, max]
  },
  "settings": [
    {
      "key": "string",
      "type": "'float' | 'int' | 'bool' | 'string'",
      "default": "value",
      "min": "number?", "max": "number?", "step": "number?",
      "unit": "string?",
      "description": "string",
      "persist": "bool=true"
    }
  ]
}
```

### hardware_type → секція board.json

| hardware_type  | board.json секція | Приклади |
|---------------|-------------------|----------|
| `gpio_output` | `gpio_outputs`    | Реле, клапани, LED |
| `gpio_input`  | `gpio_inputs`     | Кнопки, кінцевики |
| `onewire_bus` | `onewire_buses`   | DS18B20 |
| `adc_channel` | `adc_channels`    | NTC, датчик тиску |
| `pwm_channel` | `pwm_channels`    | Вентилятори |
| `i2c_bus`     | `i2c_buses`       | BME280, дисплеї |

### Приклад: DS18B20

```json
{
  "driver": "ds18b20",
  "description": "Dallas DS18B20 цифровий датчик температури (OneWire)",
  "category": "sensor",
  "hardware_type": "onewire_bus",
  "provides": { "type": "float", "unit": "°C", "range": [-55, 125] },
  "settings": [
    {"key": "read_interval_ms", "type": "int",   "default": 1000, "min": 500,  "max": 60000, "step": 100, "unit": "мс", "description": "Інтервал опитування"},
    {"key": "offset",           "type": "float", "default": 0.0,  "min": -5.0, "max": 5.0,   "step": 0.1, "unit": "°C", "description": "Корекція показань"},
    {"key": "resolution",       "type": "int",   "default": 12,   "min": 9,    "max": 12,    "step": 1,   "unit": "біт", "description": "Роздільна здатність"}
  ]
}
```

### Приклад: Relay

```json
{
  "driver": "relay",
  "description": "Дискретне реле (GPIO output on/off)",
  "category": "actuator",
  "hardware_type": "gpio_output",
  "provides": { "type": "bool" },
  "settings": [
    {"key": "min_off_time_s", "type": "int", "default": 180, "min": 0, "max": 600, "step": 10, "unit": "с", "description": "Мін. час вимкнення (захист компресора)"},
    {"key": "min_on_time_s",  "type": "int", "default": 60,  "min": 0, "max": 600, "step": 10, "unit": "с", "description": "Мін. час увімкнення"}
  ]
}
```

### Приклад: NTC термістор

```json
{
  "driver": "ntc",
  "description": "NTC термістор через ADC канал",
  "category": "sensor",
  "hardware_type": "adc_channel",
  "provides": { "type": "float", "unit": "°C", "range": [-40, 150] },
  "settings": [
    {"key": "beta",             "type": "int",   "default": 3950,  "min": 2000, "max": 5000,   "description": "B-коефіцієнт NTC"},
    {"key": "r_nominal",        "type": "int",   "default": 10000, "min": 1000, "max": 100000, "unit": "Ом", "description": "Номінальний опір при 25°C"},
    {"key": "r_series",         "type": "int",   "default": 10000, "min": 1000, "max": 100000, "unit": "Ом", "description": "Опір послідовного резистора"},
    {"key": "read_interval_ms", "type": "int",   "default": 1000,  "min": 100,  "max": 60000,  "unit": "мс", "description": "Інтервал опитування"}
  ]
}
```

### Приклад: Дискретний вхід

```json
{
  "driver": "digital_input",
  "description": "Дискретний вхід (двері, кінцевик, кнопка)",
  "category": "sensor",
  "hardware_type": "gpio_input",
  "provides": { "type": "bool" },
  "settings": [
    {"key": "debounce_ms", "type": "int",  "default": 50,    "min": 0, "max": 1000, "step": 10, "unit": "мс", "description": "Час антибрязкоту"},
    {"key": "invert",      "type": "bool", "default": false,  "description": "Інвертувати логіку"}
  ]
}
```

### Правила driver manifest

- **driver** — збігається з назвою папки в `drivers/`
- **category** — `sensor` → ISensorDriver, `actuator` → IActuatorDriver
- **hardware_type** — ключ без `s`: `gpio_output` → `gpio_outputs` в board.json
- **settings[].key** — namespace в NVS: `<hardware_id>.<key>`

---

## 3. Module Manifest (`modules/<name>/manifest.json`)

Описує бізнес-логіку модуля: які апаратні ролі потребує, які state keys створює,
як виглядає в WebUI, що публікує/підписується в MQTT, що показує на дисплеї.

Це **головний** маніфест з точки зору UI Generation Pipeline.
Python-генератор (`tools/generate_ui.py`) читає саме ці файли.

### Schema

```json
{
  "manifest_version": 1,
  "module": "string — збігається з назвою папки",
  "description": "string — опис англійською для документації",

  "requires": [
    {
      "role": "string — логічна роль (chamber_temp, compressor)",
      "type": "'sensor' | 'actuator'",
      "driver": "string — назва драйвера (ds18b20, relay, ntc)"
    }
  ],

  "state": {
    "<module>.<key>": {
      "type": "'float' | 'int' | 'bool' | 'string'",
      "unit": "string? — одиниці виміру (°C, %, dBm)",
      "access": "'read' | 'readwrite'",
      "min": "number? — обов'язково для readwrite float/int",
      "max": "number? — обов'язково для readwrite float/int",
      "step": "number? — обов'язково для readwrite float/int",
      "default": "value? — початкове значення",
      "persist": "bool? — зберігати в NVS (default: false)",
      "enum": ["string[]? — допустимі значення для string type"],
      "description": "string — опис українською для UI"
    }
  },

  "ui": {
    "page": "string — заголовок сторінки (українською)",
    "page_id": "string? — id сторінки (default: module name)",
    "icon": "string? — ім'я іконки (snowflake, flame, fan)",
    "order": "int? — порядок в навігації (default: 10)",
    "cards": [
      {
        "title": "string — заголовок картки",
        "group": "string? — 'settings' для групи налаштувань",
        "collapsible": "bool? — чи можна згорнути (default: false)",
        "widgets": [
          {
            "key": "string — state key з секції state",
            "widget": "string — тип віджету (gauge, slider, etc.)",
            "size": "'small' | 'medium' | 'large'?",
            "...": "додаткові параметри залежно від widget type"
          }
        ]
      }
    ]
  },

  "mqtt": {
    "publish": ["string[] — state keys для публікації"],
    "subscribe": ["string[] — state keys для підписки (readwrite)"]
  },

  "display": {
    "main_value": {
      "key": "string — state key для головного значення",
      "format": "string — printf format (%.1f°C)"
    },
    "menu_items": [
      {
        "label": "string — підпис (українською)",
        "key": "string — state key для редагування"
      }
    ]
  }
}
```

### Widget Type Reference

| Widget | Сумісні types | Потребує state access | Додаткові параметри |
|--------|--------------|----------------------|---------------------|
| `gauge` | float, int | read | `size`, `color_zones: [{to, color}]` |
| `slider` | float, int | readwrite | `min`, `max`, `step` (з state) |
| `number_input` | float, int | readwrite | `min`, `max`, `step` (з state) |
| `indicator` | bool | read | `on_label`, `off_label`, `on_color`, `off_color` |
| `toggle` | bool | readwrite | `on_label`, `off_label` |
| `status_text` | string | read | `enum` (з state) |
| `value` | float, int, bool, string | read | `unit` (з state) |
| `chart` | float | read | `history_size`, `interval_s` |
| `button` | — | — | `label`, `api_endpoint`, `confirm` |
| `text_input` | string | readwrite | `api_endpoint` |
| `password_input` | string | readwrite | `api_endpoint` |
| `select` | string | readwrite | `enum` (з state) |

**Примітки:**
- Якщо widget пов'язаний зі state key, параметри `unit`, `min`, `max`, `step`, `description`
  автоматично підтягуються з `state` секції — не дублюй їх у widget.
- `button`, `text_input`, `password_input` — системні віджети, зазвичай не в module manifest.
- `select` — заплановано, ще не реалізовано в генераторі.

### Правила module manifest

- **module** — ПОВИНЕН збігатися з назвою папки в `modules/`
- **State key format** — `<module>.<key>`, наприклад `thermostat.temperature`
- **readwrite float/int** — ОБОВ'ЯЗКОВО мають `min`, `max`, `step`
- **readwrite bool** — `min`/`max`/`step` не потрібні
- **persist: true** — значення зберігається в NVS і відновлюється при boot
- **UI widget key** — ПОВИНЕН існувати в `state` секції (валідація генератором)
- **MQTT keys** — ПОВИННІ існувати в `state` секції
- **MQTT subscribe** — тільки `readwrite` state keys
- **Display keys** — ПОВИННІ існувати в `state` секції
- **order** — менше число = вище в навігації. System pages: network=90, system=99

### Приклад: Thermostat (холодильна камера)

Повний приклад — `modules/thermostat/manifest.json`. Ключові моменти:

```json
{
  "manifest_version": 1,
  "module": "thermostat",
  "description": "On/off thermostat with hysteresis regulation for cold rooms",

  "requires": [
    {"role": "chamber_temp", "type": "sensor",   "driver": "ds18b20"},
    {"role": "compressor",   "type": "actuator", "driver": "relay"}
  ],

  "state": {
    "thermostat.temperature": {
      "type": "float", "unit": "°C", "access": "read",
      "description": "Поточна температура камери"
    },
    "thermostat.setpoint": {
      "type": "float", "unit": "°C", "access": "readwrite",
      "min": -35.0, "max": 0.0, "step": 0.5,
      "default": -18.0, "persist": true,
      "description": "Уставка температури"
    }
  },

  "ui": {
    "page": "Холодильна камера",
    "icon": "snowflake",
    "order": 1,
    "cards": [
      {
        "title": "Температура",
        "widgets": [
          {"key": "thermostat.temperature", "widget": "gauge", "size": "large",
           "color_zones": [{"to": -22, "color": "#3b82f6"}, {"to": -15, "color": "#22c55e"}]},
          {"key": "thermostat.setpoint", "widget": "slider"}
        ]
      }
    ]
  },

  "mqtt": {
    "publish":   ["thermostat.temperature", "thermostat.compressor", "thermostat.state"],
    "subscribe": ["thermostat.setpoint", "thermostat.hysteresis"]
  }
}
```

---

## 4. Bindings Manifest (`data/bindings.json`)

Описує рантайм зв'язки: яке hardware з board.json прив'язане до якої ролі модуля
і через який драйвер.

Це єдиний маніфест який **оператор** може редагувати через UI (Phase 7+).
Поки створюється вручну.

### Schema

```json
{
  "bindings": [
    {
      "hardware": "string — id з board.json (relay_1, ow_1)",
      "role": "string — роль з module manifest requires",
      "driver": "string — драйвер з drivers/",
      "module": "string — модуль-власник ролі"
    }
  ]
}
```

### Приклад

```json
{
  "bindings": [
    {"hardware": "relay_1", "role": "compressor",   "driver": "relay",   "module": "thermostat"},
    {"hardware": "ow_1",    "role": "chamber_temp", "driver": "ds18b20", "module": "thermostat"}
  ]
}
```

### Як працює зв'язування

```
Module manifest requires:           bindings.json:              Board manifest:
  role: compressor           →      hardware: relay_1      →    gpio_outputs[relay_1]: GPIO14
  type: actuator                    driver: relay
  driver: relay                     module: thermostat
```

При boot HAL/DriverManager:
1. Читає `board.json` → знає які GPIO доступні
2. Читає `bindings.json` → знає яке hardware → якому модулю
3. Створює драйвер з правильним GPIO
4. Модуль отримує драйвер через `role` ім'я

### Правила bindings manifest

- **hardware** — ПОВИНЕН існувати в `board.json` (в відповідній секції)
- **driver** — ПОВИНЕН існувати в `drivers/` та мати manifest
- **module** — ПОВИНЕН бути в `project.json → modules[]`
- **role** — ПОВИНЕН існувати в `module manifest → requires[]`
- **driver.hardware_type** — ПОВИНЕН відповідати секції board.json де знаходиться hardware id
- Одне hardware може мати **тільки один** binding (relay_1 не може бути і compressor, і fan)
- Одна role може мати **тільки один** binding
- Всі `requires[]` модуля ПОВИННІ мати binding (інакше модуль не стартує)

### Cross-manifest валідація

| Перевірка | Що | Де |
|-----------|----|----|
| hardware exists | `binding.hardware` ∈ `board.json[section]` | boot |
| driver exists | `binding.driver` ∈ `drivers/` | build |
| module active | `binding.module` ∈ `project.json.modules` | build |
| role exists | `binding.role` ∈ `module.requires[].role` | build |
| type match | `binding.driver.hardware_type` → `board.json` section | boot |
| no duplicate hw | кожен `hardware` тільки в одному binding | boot |
| no duplicate role | кожна `role` тільки в одному binding | boot |
| all roles bound | всі `module.requires` покриті bindings | boot |

---

## 5. Validation Summary

Зведена таблиця всіх перевірок які виконує система (build-time та boot-time).

### Build-time валідація (generate_ui.py)

| # | Перевірка | Severity | Опис |
|---|-----------|----------|------|
| V1 | manifest_version | error | Маніфест повинен мати `manifest_version: 1` |
| V2 | module field exists | error | Поле `module` обов'язкове |
| V3 | state field exists | error | Поле `state` обов'язкове |
| V4 | state key has type | error | Кожен state key повинен мати `type` |
| V5 | state key has access | error | Кожен state key повинен мати `access` |
| V6 | readwrite has min/max/step | error | readwrite float/int потребує `min`, `max`, `step` |
| V7 | widget key exists | error | Widget `key` повинен існувати в `state` |
| V8 | widget type compatible | error | Widget type сумісний зі state type (WIDGET_TYPE_COMPAT) |
| V9 | mqtt publish key exists | error | MQTT publish key повинен існувати в `state` |
| V10 | mqtt subscribe key exists | error | MQTT subscribe key повинен існувати в `state` |
| V11 | display key exists | error | Display main_value/menu_item key повинен існувати в `state` |
| V12 | no duplicate state keys | error | Один state key не може бути в двох модулях |
| V13 | state key prefix | warning | State key повинен починатися з `<module>.` |

### Boot-time валідація (HAL / DriverManager)

| # | Перевірка | Severity | Опис |
|---|-----------|----------|------|
| B1 | hardware exists | error | `binding.hardware` існує в `board.json` |
| B2 | driver type match | error | `driver.hardware_type` відповідає board.json секції |
| B3 | no duplicate hardware | error | Один hardware id → один binding |
| B4 | all roles bound | warning | Всі `module.requires[]` мають binding |

### Manifest взаємозв'язки

```
project.json ──── modules[] ────→ modules/<name>/manifest.json
                                      │
                                      ├── state{} ←─── ui.cards[].widgets[].key
                                      │            ←─── mqtt.publish[]
                                      │            ←─── mqtt.subscribe[]
                                      │            ←─── display.main_value.key
                                      │            ←─── display.menu_items[].key
                                      │
                                      └── requires[] ←── bindings.json[].role

board.json ──── gpio_outputs[] ──→ bindings.json[].hardware
           ├── onewire_buses[]
           ├── adc_channels[]
           └── gpio_inputs[]

drivers/<name>/manifest.json ──→ bindings.json[].driver
```

---

## Changelog

- v1.0 (2026-02-16) — Board manifest та Driver manifest
- v2.0 (2026-02-16) — Module manifest, Bindings manifest, Validation Summary
