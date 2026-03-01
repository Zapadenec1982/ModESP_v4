# WISHLIST — Ідеї та побажання для ModESP

> Збірник ідей для майбутніх фаз. Не привʼязані до конкретних дедлайнів.
> Переносяться в CLAUDE_TASK коли приходить час реалізації.

---

## 🌡️ Weather-based Setpoint Offset

**Ідея:** Раз на добу завантажувати прогноз погоди, і залежно від температури
зміщувати setpoint. Спекотний день → більше відкривань вітрини → переохолодити завчасно.

**Реалізація:**
- Один HTTPS запит на добу (~40KB RAM на 2-3 секунди, потім звільняється)
- Weather API: OpenWeatherMap (free tier, 1000 req/day) або інший
- Параметри: `weather_api_key`, `weather_city`, `weather_offset_per_degree`
- Формула: `weather_offset = (forecast_temp - 20) * offset_per_degree`
- Альтернатива: отримувати offset через MQTT від зовнішнього сервісу
- effective_setpoint = setpoint + night_offset + weather_offset

**Натхнення:** Coca-Cola / Red Bull smart coolers, IoTManager weather module

**Складність:** Низька. ~150 рядків (HTTP client + JSON parse + timer)

---

## 📱 Telegram Notifications

**Ідея:** Надсилати сповіщення про критичні події в Telegram.

**Коли сповіщати:**
- HAL/LAL alarm спрацював
- Sensor offline (5 consecutive errors)
- Defrost timeout (heater_alarm)
- Door open > N хвилин
- Power restored after outage
- Low memory warning

**Реалізація:**
- TelegramService: черга повідомлень (5-10 × 100 bytes = 1KB RAM)
- HTTPS POST на `api.telegram.org/bot{token}/sendMessage`
- ~40KB RAM на 2-3 секунди per message, потім звільняється
- Параметри: `telegram.bot_token`, `telegram.chat_id`, `telegram.enabled`
- 2-5 повідомлень на день максимум
- Не потребує polling (тільки send)

**Натхнення:** GyverLibs/FastBot — підхід до мінімального API.
FastBot2 — новіша версія. Алгоритми цікаві, але реалізація на ESP-IDF своя.

**Складність:** Низька. ~150-200 рядків

---

## 🔧 Бібліотеки AlexGyver — ідеї для алгоритмів

Не для прямого використання (вони Arduino), а як reference:

| Бібліотека | Що взяти | Для якої фази |
|-----------|---------|---------------|
| **GyverPID** | PID auto-tune, anti-windup, adaptive | Phase 11c (PID регулювання) |
| **GyverNTC** | Формула Стейнхарта-Харта, B-коефіцієнти | ~~Phase 11c~~ (РЕАЛІЗОВАНО — NTC driver, Phase 11b) |
| **GyverOLED** | SSD1306 I2C display patterns | Phase 8 (LCD/OLED) |
| **GyverDS18** | SEARCH_ROM алгоритм | ~~Phase 11b~~ (РЕАЛІЗОВАНО — SEARCH_ROM, Phase 11b) |
| **FastBot2** | Telegram Bot API мінімалістичний підхід | Telegram notifications |

---

## 🖥️ IoTManager Integration

**Проект:** https://github.com/IoTManagerProject/IoTManager

**Ідея:** IoTManager як зовнішній менеджер — погода, Telegram, сценарії.
ModESP публікує дані в MQTT, IoTManager підписується і керує.

**Статус:** Проект не дуже активний (останній реліз 2021), але концепція цікава.
ModESP вже сумісний через MQTT — нічого додаткового не потрібно.

---

## 🚀 ESP32-S3 + PSRAM

**Ідея:** Міграція на ESP32-S3 з PSRAM для розширених можливостей.

**Що дає:**
- 520KB SRAM + 2-8MB PSRAM (×10 памʼяті)
- DataLogger в RAM (24h × 1sec = ~350KB)
- HTTPS server (TLS ~40KB per connection)
- Більше WebSocket клієнтів (8-10 замість 4)
- Lua/JS scripting для кастомної логіки
- Нативний USB (без CH340)

**Що змінити:** sdkconfig (target + SPIRAM) + board.json (GPIO pinout).
Бізнес-модулі без змін.

**Коли:** Коли зʼявиться конкретна потреба (DataLogger, HTTPS, scripting).

---

## 🐧 Linux + MCU архітектура

**Ідея:** Raspberry Pi (або Orange Pi) як надбудова над ESP32.

**Linux відповідає за:**
- Dashboard з графіками (Grafana)
- PostgreSQL/SQLite (історія за роки)
- HTTPS + Let's Encrypt
- Multi-user auth, RBAC
- VPN, remote access
- Telegram/Email alerts (складніші сценарії)
- OTA management для N контролерів
- Камери, ML (predictive maintenance)

**ESP32 відповідає за:**
- Real-time control (як зараз)
- Автономна робота при падінні Linux
- Fallback WebUI

**Звʼязок:** MQTT (вже є), REST API (вже є), RS485/Modbus (Phase 13)

**Коли:** Далека перспектива. Після завершення бази (Phase 11-13).

---

## ~~📊 DataLogger з графіками~~ — РЕАЛІЗОВАНО (Phase 14+14b)

**Статус:** Повністю реалізовано. 6-channel dynamic DataLogger з LittleFS,
streaming chunked JSON, ChartWidget (SVG Catmull-Rom), CSV export, event logging.
Деталі: docs/09_datalogger.md

---

## 🔌 Coordinated Defrost через MQTT

**Ідея:** Master контролер координує час відтайки для групи вітрин.

**Деталі:** див. Phase 11d опис в попередніх обговореннях.

**Коли:** Коли зʼявиться інсталяція з 2+ контролерами.

---

## Changelog
- 2026-03-01 — Оновлено: DataLogger та GyverDS18/NTC помічені як реалізовані
- 2026-02-21 — Створено. Зібрано ідеї з обговорень.
