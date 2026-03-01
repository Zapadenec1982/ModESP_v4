# Production Sprint 6 — LCD/OLED Display (3 сесії)

## Контекст

Генератор вже створює `generated/display_screens.h` з даними для дисплею.
Потрібно: I2C OLED driver + renderer + button navigation.

## Сесія 6a: SSD1306 I2C OLED Driver

### Задачі
- [ ] Створити driver `drivers/ssd1306/` (manifest + src)
- [ ] I2C init (SDA/SCL з board.json, address 0x3C default)
- [ ] Framebuffer 128×64 (1024 bytes)
- [ ] Basic primitives: clear, pixel, line, rect, text (5×7 font)
- [ ] `display_service` component: init, update, draw
- [ ] HAL integration: display driver в DriverManager

### Файли
- `drivers/ssd1306/manifest.json` — СТВОРИТИ
- `drivers/ssd1306/src/ssd1306_driver.cpp` — СТВОРИТИ
- `components/modesp_hal/` — display interface

## Сесія 6b: Screen Renderer + Navigation

### Задачі
- [ ] Використати `display_screens.h` для визначення екранів
- [ ] Головний екран: температура (великий шрифт) + стан (COOLING/IDLE/DEFROST)
- [ ] Setpoint екран: SP значення + кнопки UP/DOWN для зміни
- [ ] Alarm екран: список активних аварій
- [ ] Info екран: WiFi IP, uptime, firmware version
- [ ] 3 кнопки (UP/DOWN/ENTER) через PCF8574 input або GPIO DigitalInput
- [ ] Button debounce (50ms, вже є в DigitalInput driver)

### Файли
- `modules/equipment/src/equipment_module.cpp` — display update в on_update()
- `components/modesp_hal/` — button reading

## Сесія 6c: Menu System + Polish

### Задачі
- [ ] Menu state machine: MAIN → SETPOINT → ALARM → INFO → MAIN (circular)
- [ ] Long press ENTER: скидання аварій (manual reset)
- [ ] Auto-return to MAIN after 30s inactivity
- [ ] Display brightness control (dimming after 5 min)
- [ ] i18n: display text UA/EN (compile-time selection)
- [ ] Host tests for menu state machine

### Критерії завершення
- [ ] SSD1306 OLED показує температуру + стан
- [ ] Навігація кнопками працює
- [ ] Setpoint змінюється з дисплея
- [ ] Аварії видимі на дисплеї
- [ ] Auto-return та dimming працюють
