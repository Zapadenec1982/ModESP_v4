# Sprint 4 / Сесія 1.7 — MQTT Backoff + WiFi Recovery

## Контекст

WebUI та security hardened (Sprints 1-3). Тепер стабільність мережі.

## Задачі

### 1. MQTT Exponential Backoff

Файл: `components/modesp_mqtt/src/mqtt_service.cpp`

Зараз: при disconnect reconnect кожні ~5с без backoff.

**Fix:**
```cpp
uint32_t reconnect_delay_ms_ = 5000;  // Initial
static constexpr uint32_t MAX_RECONNECT_DELAY_MS = 30000;

// On disconnect:
reconnect_delay_ms_ = std::min(reconnect_delay_ms_ * 2, MAX_RECONNECT_DELAY_MS);

// On successful connect:
reconnect_delay_ms_ = 5000;  // Reset
```

### 2. WiFi STA Watchdog

Файл: `components/modesp_net/src/wifi_service.cpp`

Зараз: після max retries → AP fallback. Але якщо WiFi flaps (connect→disconnect кожні 30с), пристрій ніколи не стабілізується.

**Fix:**
- Лічильник загального часу без стабільного з'єднання
- Якщо disconnected сумарно > 10 хвилин за останню годину → esp_restart()
- Configurable через define (WIFI_MAX_DISCONNECT_TOTAL_MS = 600000)

## Файли

| Файл | Дія |
|------|-----|
| `components/modesp_mqtt/src/mqtt_service.cpp` | РЕДАГУВАТИ |
| `components/modesp_net/src/wifi_service.cpp` | РЕДАГУВАТИ |

## Критерії завершення

- [ ] `idf.py build` без помилок
- [ ] MQTT: видимий backoff в логах (5s → 10s → 20s → 30s)
- [ ] WiFi: restart після тривалого disconnect

## Після завершення

```bash
idf.py build
git add components/modesp_mqtt/ components/modesp_net/
git commit -m "fix(net): MQTT exponential backoff + WiFi STA watchdog

- MQTT: 5s→10s→20s→30s backoff on disconnect, reset on connect
- WiFi: restart if disconnected >10min total in 1 hour"
git push origin main
```
