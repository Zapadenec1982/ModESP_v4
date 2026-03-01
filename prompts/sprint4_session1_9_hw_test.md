# Sprint 4 / Сесія 1.9 — Hardware Integration Test

## Контекст

Всі MVP зміни реалізовано (Sprint 1-4). Фінальна валідація на реальному KC868-A6.

## Чеклист тестування

### Dashboard (Mobile)
- [ ] Android Chrome: alarm banner зверху, pills з текстом, secondary temps
- [ ] iPhone Safari: layout без скролу, bottom tabs max 5
- [ ] Landscape: layout адаптується
- [ ] Setpoint slider: thumb зручний для пальця, debounce працює
- [ ] Equipment pills: ON/OFF стан чітко видимий

### Settings Pages
- [ ] NumberInput: rapid click +/- → 1 POST (debounce)
- [ ] Success flash після збереження
- [ ] Error: revert + toast з зрозумілим повідомленням
- [ ] SelectWidget: disabled options — grey + strikethrough
- [ ] MqttSave: заповнити форму → Save → працює (не DOM query issue)

### OTA
- [ ] Upload valid firmware.bin → success, countdown, reboot
- [ ] Upload non-.bin file → error "Not a valid ESP32 firmware"
- [ ] Upload >1.5MB file → error "Firmware too large"
- [ ] Rollback: flash bad firmware → auto-rollback через 60с

### HTTP Auth
- [ ] POST /api/settings без auth → 401
- [ ] Login modal → ввести admin/modesp → success
- [ ] Зміна пароля → re-login з новим
- [ ] GET /api/state, /api/ui → 200 без auth

### Connection
- [ ] Відключити WiFi → overlay через 5с → підключити → overlay зникає, toast "З'єднано"
- [ ] Закрити WS вручну → overlay → reconnect
- [ ] 3 вкладки одночасно → всі отримують updates

### Memory (48+ годин)
- [ ] Flash firmware → monitor serial output
- [ ] Записати initial heap_free та heap_largest
- [ ] Через 1 годину: порівняти heap (різниця < 1KB = стабільно)
- [ ] Через 24 години: heap_largest не впав нижче 30KB
- [ ] Через 48 годин: жодних WS heap guard warnings в логах
- [ ] WS payload size в DevTools: ~200 байт (delta), не 3.5KB

### MQTT
- [ ] Підключитись до HiveMQ Cloud → pub/sub працює
- [ ] Відключити broker → логи показують backoff (5s→10s→20s→30s)
- [ ] Підключити broker → reconnect, backoff reset

### °C/°F
- [ ] Toggle одиниці на System page
- [ ] Dashboard: температура в °F коли обрано
- [ ] Settings: setpoint/limits показуються в °F
- [ ] Зберегти setpoint → внутрішнє значення залишається в °C

### Alarm Relay
- [ ] Прив'язати alarm_relay до реле через bindings
- [ ] Створити alarm (high temp) → реле ON
- [ ] Clear alarm → реле OFF

## Результат

Після проходження всіх тестів:
- [ ] Оновити ACTION_PLAN.md — MVP Sprint 1-4 завершено
- [ ] Оновити CLAUDE.md якщо змінилась архітектура
- [ ] Commit: "test: MVP hardware integration test PASSED on KC868-A6"
- [ ] Push to main

**MVP DONE!** Пристрій готовий до field deployment під наглядом інженера.
