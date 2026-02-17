# ModESP v4

**Modular ESP32 firmware framework for industrial refrigeration controllers**

![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.5-blue)
![C++17](https://img.shields.io/badge/C%2B%2B-17-orange)
![License](https://img.shields.io/badge/License-GPL--3.0-green)
![Platform](https://img.shields.io/badge/Platform-ESP32--WROOM--32-red)

---

## What is this?

ModESP v4 is an open-source firmware framework that turns a cheap ESP32 module into a professional-grade industrial controller for refrigeration and HVAC equipment. It replaces expensive proprietary controllers (Danfoss, Dixell) with a modular, extensible, and maintainable solution.

### Key Features

- **Modular architecture** -- plug-in modules with lifecycle management and priority-based initialization
- **Manifest-driven code generation** -- single source of truth: JSON manifests auto-generate UI, C++ headers, MQTT topics
- **Zero heap in hot path** -- ETL (Embedded Template Library) instead of STL; no `new`/`malloc` in runtime loops
- **Web UI** -- built-in HTTP server with real-time WebSocket state broadcast
- **MQTT with TLS** -- publish state and subscribe to commands via any MQTT broker
- **OTA updates** -- over-the-air firmware upload with rollback support
- **Persistent settings** -- auto-save configuration to NVS with debounce
- **WiFi** -- STA mode (connect to router) with AP fallback (ModESP-XXXX)
- **Hardware abstraction** -- DS18B20 temperature sensors (OneWire), GPIO relays with min on/off protection

---

## Architecture

```
 module manifest.json  ──┐
 driver manifest.json  ──┼──▶  generate_ui.py  ──▶  ui.json (WebUI schema)
 board.json            ──┤                     ──▶  state_meta.h (C++ constexpr)
 bindings.json         ──┘                     ──▶  mqtt_topics.h
                                               ──▶  display_screens.h
```

The Python generator (`tools/generate_ui.py`, ~900 lines) reads all manifests and produces 4 artifacts. These are regenerated on every build -- never edit them manually.

---

## Project Structure

```
ModESP_v4/
├── main/main.cpp                  # Boot sequence, main loop (3 init phases)
├── components/
│   ├── modesp_core/               # BaseModule, ModuleManager, SharedState, types
│   ├── modesp_services/           # Error, Watchdog, Logger, Config, NVS, Persist
│   ├── modesp_hal/                # HAL, DriverManager, driver interfaces
│   ├── modesp_net/                # WiFi, HTTP (12 endpoints), WebSocket
│   ├── modesp_mqtt/               # MQTT client with TLS
│   ├── modesp_json/               # JSON serialization helpers
│   └── jsmn/                      # Lightweight JSON parser (header-only)
├── drivers/
│   ├── ds18b20/                   # Dallas DS18B20 temperature sensor (OneWire)
│   └── relay/                     # GPIO relay with min on/off time protection
├── modules/
│   └── thermostat/                # On/Off regulation with hysteresis
├── tools/
│   ├── generate_ui.py             # Manifest → artifacts generator
│   └── tests/                     # 79 pytest tests
├── data/
│   ├── board.json                 # PCB pin assignment
│   ├── bindings.json              # Runtime: role → driver → GPIO mapping
│   └── www/                       # Static WebUI (index.html, app.js, style.css)
├── docs/                          # Architecture documentation
├── project.json                   # Active modules configuration
├── partitions.csv                 # Flash partition table (NVS + app + LittleFS)
├── sdkconfig.defaults             # Minimal ESP-IDF config
└── CMakeLists.txt                 # Build config (auto-runs generator)
```

---

## Quick Start

### Prerequisites

- [ESP-IDF v5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/get-started/)
- Python 3.8+
- ESP32-WROOM-32 board

### Build & Flash

```bash
# Clone
git clone https://github.com/<your-username>/ModESP_v4.git
cd ModESP_v4

# Set target
idf.py set-target esp32

# Build (auto-runs generate_ui.py)
idf.py build

# Flash and monitor (adjust COM port)
idf.py -p COM15 flash monitor
```

### Run Tests

```bash
pytest tools/tests/ -v
```

---

## Configuration

| File | Purpose |
|------|---------|
| `project.json` | Active modules and system pages |
| `data/board.json` | PCB pin assignment (GPIO outputs, OneWire buses) |
| `data/bindings.json` | Runtime mapping: role → driver → GPIO |
| `modules/*/manifest.json` | Module definition (UI, state keys, MQTT topics) |
| `drivers/*/manifest.json` | Driver definition (category, hardware type, settings) |
| `sdkconfig.defaults` | ESP-IDF configuration overrides |

---

## HTTP API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/state` | GET | Full SharedState as JSON |
| `/api/ui` | GET | UI schema (generated from manifests) |
| `/api/settings` | POST | Change writable state keys (with min/max validation) |
| `/api/board` | GET | Board configuration |
| `/api/bindings` | GET | Driver bindings |
| `/api/modules` | GET | Module list and status |
| `/api/mqtt` | GET/POST | MQTT config and status |
| `/api/wifi` | POST | WiFi credentials |
| `/api/wifi/scan` | GET | WiFi scan results |
| `/api/ota` | GET/POST | Firmware info / OTA upload |
| `/api/restart` | POST | Restart ESP32 |
| `/ws` | WS | Real-time state broadcast |

---

## Tech Stack

| Component | Technology |
|-----------|-----------|
| MCU | ESP32-WROOM-32, 4MB flash |
| Framework | ESP-IDF v5.5 |
| Language | C++17 |
| Containers | ETL (Embedded Template Library) |
| JSON parser | jsmn (header-only) |
| Filesystem | LittleFS |
| RTOS | FreeRTOS |
| Code generation | Python 3 |
| Testing | pytest (79 tests) |

---

## Current Status

**Phase 6.5 complete.** The following is fully operational:

- 10+ system modules initialized at boot
- Thermostat module with on/off hysteresis control
- DS18B20 sensor reading, relay control
- WiFi (STA + AP fallback)
- HTTP REST API (12 endpoints)
- WebSocket real-time broadcast (max 4 clients)
- MQTT with TLS support
- OTA firmware update with rollback
- Settings auto-persist to NVS (5s debounce)
- Free heap after boot: **177 KB** / 240 KB

---

## License

This project is licensed under the **GNU General Public License v3.0** -- see the [LICENSE](LICENSE) file for details.

---

## Українською

### Що таке ModESP v4?

ModESP v4 -- модульний фреймворк прошивки для промислових ESP32-контролерів холодильного обладнання. Замінює дорогі контролери Danfoss/Dixell дешевим модулем ESP32 з професійною архітектурою.

### Для кого?

- OEM-виробники холодильного обладнання (UBC Group, Modern Expo)
- Сервісні інженери
- Інтегратори промислового обладнання

### Як почати?

1. Встановіть [ESP-IDF v5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/get-started/)
2. Клонуйте репозиторій: `git clone <url>`
3. Зберіть проект: `idf.py build`
4. Прошийте ESP32: `idf.py -p COM15 flash monitor`
5. Підключіться до WiFi точки `ModESP-XXXX` та відкрийте `192.168.4.1`

### Документація

Уся внутрішня документація та коментарі в коді -- **українською мовою**. Архітектурні документи знаходяться в папці `docs/`.

### Технічні особливості

- Нуль алокацій в гарячому шляху (ETL замість STL)
- Автогенерація UI, MQTT-топіків та C++ метаданих з маніфестів
- Персистентність налаштувань в NVS з дебаунсом
- OTA-оновлення з підтримкою rollback
- MQTT з TLS для інтеграції з SCADA/HMI
