# Промпт для Claude Code — Phase 12a: KC868-A6 Board Support

Прочитай CLAUDE.md для контексту проекту.

Прочитай CLAUDE_TASK_12a.md — там повне завдання Phase 12a з 8-ма задачами.

## Що робити

Додати підтримку плати KinCony KC868-A6 — промислова ESP32 плата де реле та
digital inputs підключені через I2C expander PCF8574, а не через GPIO напряму.

Прошивка залишається УНІВЕРСАЛЬНОЮ — board.json визначає яка плата. Одна прошивка,
різні конфіги. Бізнес-модулі (equipment, thermostat, defrost, protection) — БЕЗ ЗМІН.

### KC868-A6 hardware

- 6 реле: I2C expander PCF8574 адреса 0x24 + ULN2003A (pins 0-5)
- 6 DI: I2C expander PCF8574 адреса 0x22, оптоізоляція (pins 0-5)
- 2 OneWire: GPIO 32, GPIO 33 (pull-up 4.7K на платі)
- 4 ADC: GPIO 36, 39, 34, 35 (0-5V)
- I2C bus: SDA=4, SCL=15

### Ключові зміни

**1. hal_types.h** — нові struct:
- I2CBusConfig, I2CExpanderConfig, I2CExpanderOutputConfig, I2CExpanderInputConfig
- I2CBusResource, I2CExpanderResource (з write_state/read_state для PCF8574)
- Додати в BoardConfig: i2c_buses, i2c_expanders, expander_outputs, expander_inputs

**2. hal.h/hal.cpp** — I2C init:
- init_i2c(): i2c_param_config + i2c_driver_install
- init_i2c_expanders(): створити I2CExpanderResource, write initial state (all OFF)
- find_i2c_expander(), find_expander_output(), find_expander_input()

**3. drivers/pcf8574_relay/** — НОВИЙ драйвер:
- Реалізує IActuatorDriver (та сама абстракція що RelayDriver)
- configure(role, expander*, pin, active_high)
- set(state): змінити біт в expander->output_state → expander->write_state()
- Кілька реле на одному expander — спільний output_state byte

**4. drivers/pcf8574_input/** — НОВИЙ драйвер:
- Реалізує ISensorDriver
- update(): раз на 100ms — expander->read_state() → витягти свій біт
- read(value): 1.0f = active, 0.0f = inactive
- KC868-A6 inputs: LOW = active (оптоізоляція, 12V logic)

**5. driver_manager.cpp** — нові driver pools + create:
- "pcf8574_relay" → PCF8574RelayDriver (знайти expander через HAL)
- "pcf8574_input" → PCF8574InputDriver

**6. config_service.cpp** — парсити нові секції з board.json:
- "i2c_buses", "i2c_expanders", "expander_outputs", "expander_inputs"
- Відсутність цих секцій — не помилка (dev board без I2C)

**7. Board configs** — data/board_kc868a6.json + data/bindings_kc868a6.json

**8. Тести** — парсинг board config, driver manifests

### Порядок роботи

1. hal_types.h → hal.h → hal.cpp (I2C types + init)
2. drivers/pcf8574_relay/ (manifest, .h, .cpp, CMakeLists)
3. drivers/pcf8574_input/ (manifest, .h, .cpp, CMakeLists)
4. driver_manager.cpp (pools + create)
5. config_service.cpp (парсинг нових секцій)
6. board_kc868a6.json + bindings_kc868a6.json
7. CMakeLists.txt (підключити нові компоненти)
8. Тести: `python -m pytest tools/tests/ -v`
9. Build: `idf.py build`

### Обмеження

- НЕ чіпай equipment_module, thermostat, defrost, protection — НУЛЬ змін
- НЕ чіпай існуючий relay_driver та ds18b20_driver
- НЕ чіпай WebUI, HTTP/WS/MQTT services
- Деталі реалізації, приклади коду — в CLAUDE_TASK_12a.md

### Після завершення

- Оновити CLAUDE.md (нові драйвери, KC868-A6 підтримка)
- Оновити ACTION_PLAN.md (Phase 12a DONE)
- Перевірити що dev board конфіг працює як раніше (backward compat)
