# Промпт для Claude Code — Phase 11b: OneWire Multi-Sensor

Прочитай CLAUDE.md для контексту проекту.

Прочитай CLAUDE_TASK_11b.md — там повне завдання Phase 11b з 8-ма задачами.

## Що робити

Реалізуй підтримку кількох DS18B20 на одній OneWire шині з auto-learn через WebUI.

### Ключові зміни

**1. Binding struct** (`components/modesp_hal/include/modesp/hal/hal_types.h`):
Додати поле `etl::string<24> address` в struct Binding.

**2. Config parser** (`components/modesp_services/src/config_service.cpp`):
Парсити "address" з bindings.json. Додати метод save_bindings() для запису.

**3. DS18B20 Driver** (`drivers/ds18b20/`):
- configure() приймає hex address (nullable)
- write_address(): MATCH_ROM (0x55) якщо є адреса, SKIP_ROM (0xCC) якщо немає
- Замінити SKIP_ROM в start_conversion() і read_scratchpad() на write_address()
- Статичний scan_bus(): SEARCH_ROM (0xF0) алгоритм (Maxim AN187)
- Статичний read_temp_by_address(): для preview при скануванні

**4. DriverManager** (`components/modesp_hal/src/driver_manager.cpp`):
Передати binding.address в ds18b20 configure().

**5. HTTP endpoints** (в http_handlers або окремий файл):
- GET /api/onewire/scan?bus=ow_1 → JSON з адресами, температурами, статусами
- POST /api/onewire/assign → {bus, address, role, module} → update bindings.json
- POST /api/onewire/unassign → {address} → remove binding

**6. WebUI** — нова сторінка або секція для OneWire auto-learn:
- Кнопка "Сканувати" → показати датчики з T та статусом (assigned/new)
- Dropdown вільних ролей для нових датчиків
- Кнопки "Призначити" / "Відв'язати"
- Банер "Restart needed" після змін

### Критична логіка

**SEARCH_ROM (0xF0)** — 1-Wire binary search, стандартний алгоритм:
- Для кожного з 64 біт: read bit, read complement, write direction
- При конфлікті (00) — запам'ятати позицію, обрати гілку
- CRC8 верифікація кожної знайденої адреси
- Повторювати поки є конфлікти (last_discrepancy)
- Reference: Maxim/Dallas AN187 "1-Wire Search Algorithm"

**Backward compatibility:** Якщо address порожній — SKIP_ROM як раніше. Один датчик на шину без адреси працює без змін.

### Порядок роботи

1. hal_types.h — поле address в Binding
2. config_service.cpp — парсинг address + save_bindings()
3. ds18b20_driver.h/.cpp — configure(address), write_address(), scan_bus(), read_temp_by_address()
4. driver_manager.cpp — передати address
5. http_handlers — scan/assign/unassign endpoints
6. WebUI — OneWire page
7. Тести
8. Build: `idf.py build` + `cd webui && npm run build && npm run deploy`

### Обмеження

- НЕ чіпай equipment/thermostat/defrost/protection modules
- НЕ робити hot-reload DriverManager (restart after assign — OK)
- НЕ оптимізувати conversion (один SKIP_ROM convert для всіх — TODO)
- Максимум 8 датчиків на шину
- Детальні приклади коду — в CLAUDE_TASK_11b.md

### Після завершення

- Оновити CLAUDE.md (multi-sensor підтримка, нові endpoints)
- Оновити ACTION_PLAN.md (Phase 11b DONE)
- Запустити всі тести
