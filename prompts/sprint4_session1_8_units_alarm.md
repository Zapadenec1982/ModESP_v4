# Sprint 4 / Сесія 1.8 — °C/°F Units + Alarm Relay

## Контекст

Мережа стабільна (сесія 1.7). Тепер два MVP features: одиниці температури та аварійне реле.

## Задачі

### 1. °C/°F Unit Selection (AUDIT-035)

**Принцип:** Внутрішні значення ЗАВЖДИ °C. Конвертація тільки на display/MQTT рівні.

**Firmware:**
- Додати `system.temp_unit` state key (persist, int32, 0=°C, 1=°F)
- Додати в system module manifest (або equipment): persist=true, options=[{0,"°C"},{1,"°F"}]
- Формула: °F = °C × 9/5 + 32

**WebUI:**
- Створити `webui/src/lib/tempFormat.js`:
  ```javascript
  export function formatTemp(celsius, unit, decimals = 1) {
    if (unit === 1) return ((celsius * 9/5) + 32).toFixed(decimals) + ' °F';
    return celsius.toFixed(decimals) + ' °C';
  }
  ```
- Dashboard: використовувати formatTemp для hero temperature, secondary temps
- Settings pages: температурні widgets (setpoint, alarm limits) — показувати в обраних одиницях
- **НЕ конвертувати значення при збереженні** — тільки display

**MQTT:**
- Опціонально: publish в обраних одиницях (або завжди °C + unit в meta)

### 2. Alarm Relay Output (AUDIT-033)

**Equipment Manager:**
- Нова роль `alarm_relay` в equipment manifest
- `equipment.alarm_active` state key (bool, readonly)
- Логіка: будь-яка активна аварія (protection.high_temp_alarm || protection.low_temp_alarm || protection.sensor1_alarm || protection.sensor2_alarm || protection.door_alarm) → реле ON
- Equipment on_update: if (any_alarm) set_relay(alarm_relay, ON) else OFF

**Bindings:**
- alarm_relay прив'язується до relay driver (GPIO або PCF8574)
- Optional — якщо не прив'язаний, equipment.has_alarm_relay = false

## Файли

| Файл | Дія |
|------|-----|
| `modules/equipment/manifest.json` | РЕДАГУВАТИ (alarm_relay role, has_alarm_relay) |
| `modules/equipment/src/equipment_module.cpp` | РЕДАГУВАТИ (alarm relay logic) |
| `webui/src/lib/tempFormat.js` | СТВОРИТИ |
| `webui/src/pages/Dashboard.svelte` | РЕДАГУВАТИ (formatTemp) |
| `tools/generate_ui.py` | МОЖЛИВО (якщо потрібен новий widget type) |

## Критерії завершення

- [ ] `idf.py build` без помилок
- [ ] system.temp_unit toggle працює: UI показує °C або °F
- [ ] Alarm relay: будь-яка аварія → реле ON, всі clear → OFF
- [ ] equipment.has_alarm_relay = true тільки коли прив'язаний
- [ ] `pytest tools/tests/ -v` зелені
- [ ] `npm run build` < 55KB gz

## Після завершення

```bash
idf.py build
cd tools && python -m pytest tests/ -v
cd ../webui && npm run build && npm run deploy
git add modules/equipment/ webui/src/ tools/
git commit -m "feat: °C/°F unit selection (AUDIT-035) + alarm relay output (AUDIT-033)

- system.temp_unit: persist, display-only conversion (internal always °C)
- alarm_relay: equipment role, any active alarm → relay ON
- WebUI: formatTemp utility, Dashboard uses selected units"
git push origin main
```
