# Промпт для Claude Code — Phase 11a

Прочитай CLAUDE.md для контексту проекту.

Прочитай CLAUDE_TASK.md — там повне завдання Phase 11a з трьома задачами.

## Що робити

Реалізуй Phase 11a — три покращення алгоритмів охолодження на рівні Danfoss:

### 1. Night Setback (thermostat)
Додай нічний режим: вночі setpoint зміщується на +N°C (економія енергії).
- 5 нових параметрів в `modules/thermostat/manifest.json` (night_setback, night_mode, night_start, night_end, night_active)
- Feature "night_setback" з always_active=true
- Метод `is_night_active()` в thermostat_module.cpp (підтримка SNTP, DI, manual)
- `effective_setpoint = setpoint + (night ? night_setback : 0)` використовується в update_regulation()
- Публікувати `thermostat.effective_setpoint` і `thermostat.night_active`

### 2. Post-defrost alarm suppression (protection)
Після завершення відтайки блокувати HAL alarm на N хвилин (false-positive усунення).
- 1 новий параметр `protection.post_defrost_delay` (default 30 хв) в `modules/protection/manifest.json`
- В protection_module.cpp: стежити за `defrost.active` falling edge → таймер suppression
- HAL alarm check загорнути в `if (!post_defrost_suppression_)`
- LAL alarm НЕ блокувати (захист продукту)

### 3. Display during defrost (thermostat + WebUI)
Під час відтайки показувати "заморожену" T замість реальної (UX для клієнтів).
- 1 новий параметр `thermostat.display_defrost` (select: real/frozen/"-d-") в thermostat manifest
- При defrost.active → публікувати `thermostat.display_temp` (замість реальної T)
- Запам'ятовувати `frozen_temp_` при старті defrost
- В Dashboard.svelte: тайл читає display_temp, -999 → "-d-"

## Порядок роботи

1. Оновити `modules/thermostat/manifest.json` — нові параметри + feature
2. Оновити `modules/protection/manifest.json` — новий параметр
3. Запустити `python tools/generate_ui.py` — перевірити що артефакти генеруються
4. Запустити `python -m pytest tools/tests/ -v` — тести зелені
5. Оновити `modules/thermostat/include/thermostat_module.h` — нові members
6. Оновити `modules/thermostat/src/thermostat_module.cpp` — night setback + display defrost
7. Оновити `modules/protection/include/protection_module.h` — нові members
8. Оновити `modules/protection/src/protection_module.cpp` — post-defrost suppression
9. Оновити `webui/src/pages/Dashboard.svelte` — display_temp замість temperature
10. Додати тести для нових параметрів
11. Запустити всі тести: `python -m pytest tools/tests/ -v`
12. Build: `cd webui && npm run build && npm run deploy`
13. ESP32 build: `idf.py build`

## Обмеження

- НЕ чіпай equipment_module.cpp, defrost_module.cpp
- НЕ чіпай HTTP/WS/MQTT services
- НЕ додавай нових драйверів
- НЕ редагуй згенеровані файли (generated/*, data/ui.json)
- Детальні приклади коду — в CLAUDE_TASK.md

## Після завершення

- Оновити CLAUDE.md якщо змінилась кількість state keys або persist params
- Відзначити Phase 11a як DONE в ACTION_PLAN.md
- Запустити всі тести, переконатись що зелені
