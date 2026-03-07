# ModESP v4

**Modular ESP32 firmware framework for industrial refrigeration controllers**

![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.5-blue)
![C++17](https://img.shields.io/badge/C%2B%2B-17-orange)
![License](https://img.shields.io/badge/License-GPL--3.0-green)
![Platform](https://img.shields.io/badge/Platform-ESP32--WROOM--32-red)
![Platform](https://img.shields.io/badge/Platform-KC868--A6-red)

---

## What is this?

ModESP v4 is an open-source firmware framework that turns a cheap ESP32 module into a professional-grade industrial controller for refrigeration and HVAC equipment. It replaces expensive proprietary controllers (Danfoss, Dixell) with a modular, extensible, and maintainable solution.

### Key Features

- **Full refrigeration control** — thermostat with asymmetric differential, 7-phase defrost (3 types), 10 alarm monitors with CompressorTracker
- **Equipment Manager** — centralized HAL owner with arbitration (Protection > Defrost > Thermostat) and safety interlocks
- **Progressive feature disclosure** — UI shows only settings for connected equipment; select widgets with human-readable labels
- **Manifest-driven code generation** — JSON manifests → auto-generated UI, C++ headers, MQTT topics, feature flags
- **Zero heap in hot path** — ETL instead of STL; no `new`/`malloc` in runtime loops
- **6-channel DataLogger** — temperature history + events on LittleFS, streaming JSON API, SVG chart with CSV export
- **Svelte Web UI** — real-time bento-card dashboard with WebSocket, 76KB gzipped (63KB+13KB), light/dark theme, i18n UA/EN, responsive accordions
- **MQTT with TLS** — publish state and subscribe to commands via any MQTT broker
- **OTA updates** — over-the-air firmware upload with rollback support
- **Persistent settings** — auto-save configuration to NVS with debounce
- **WiFi** — STA mode (connect to router) with AP fallback (ModESP-XXXX)

---

## Architecture

```
 module manifest.json  ──┐
 driver manifest.json  ──┼──▶  generate_ui.py  ──▶  ui.json (WebUI schema)
 board.json            ──┤                     ──▶  state_meta.h (C++ metadata)
 bindings.json         ──┘                     ──▶  mqtt_topics.h
                                               ──▶  display_screens.h
                                               ──▶  features_config.h
```

### Equipment Layer

```
  Thermostat          Defrost           Protection
      │                  │                  │
      │ req.compressor   │ req.defrost_relay │ lockout
      ▼                  ▼                  ▼
  ┌──────────────────────────────────────────────┐
  │           Equipment Manager                   │
  │  Arbitration: Protection > Defrost > Thermo   │
  │  Interlocks: defrost_relay↔compressor         │
  └──────────────────────────────────────────────┘
      │           │           │           │
    Relay1      Relay2      Relay3      Relay4
    DS18B20     DS18B20     DI(door)    ...
```

Business modules never access hardware directly — they publish requests to SharedState, Equipment Manager arbitrates and drives relays.

---

## Project Structure

```
ModESP_v4/
├── main/main.cpp                  # Boot sequence, main loop (3 init phases)
├── components/
│   ├── modesp_core/               # BaseModule, ModuleManager, SharedState, types
│   ├── modesp_services/           # Error, Watchdog, Logger, Config, NVS, PersistService
│   ├── modesp_hal/                # HAL, DriverManager, driver interfaces
│   ├── modesp_net/                # WiFi, HTTP (21 endpoints), WebSocket
│   ├── modesp_mqtt/               # MQTT client with TLS
│   ├── modesp_json/               # JSON serialization helpers
│   └── jsmn/                      # Lightweight JSON parser (header-only)
├── drivers/
│   ├── digital_input/             # GPIO digital input (door contact etc.)
│   ├── ds18b20/                   # Dallas DS18B20 temperature sensor (OneWire)
│   ├── ntc/                       # NTC thermistor via ADC (B-parameter equation)
│   ├── pcf8574_input/             # I2C PCF8574 digital input (KC868-A6)
│   ├── pcf8574_relay/             # I2C PCF8574 relay output (KC868-A6)
│   └── relay/                     # GPIO relay with min on/off time protection
├── modules/
│   ├── equipment/                 # HAL owner, arbitration, interlocks
│   ├── thermostat/                # Asymmetric differential, fan control, safety run
│   ├── defrost/                   # 7-phase FSM, 3 defrost types, 4 initiations
│   ├── protection/                # 10 alarm monitors, CompressorTracker, motor hours
│   └── datalogger/                # 6-channel temperature logging + events (LittleFS)
├── tools/
│   ├── generate_ui.py             # Manifest → 5 artifacts generator (~1644 lines)
│   └── tests/                     # 264 pytest tests
├── webui/                         # Svelte 4 WebUI source
│   ├── src/                       # App.svelte, stores, components (24 widgets), pages
│   └── scripts/deploy.js          # Gzip + copy to data/www/
├── data/
│   ├── board.json                 # PCB pin assignment (gpio_outputs, onewire_buses, gpio_inputs, adc_channels, i2c_buses, i2c_expanders, expander_outputs, expander_inputs)
│   ├── bindings.json              # Runtime: role → driver → GPIO mapping
│   ├── ui.json                    # 🔄 Generated
│   └── www/                       # Deployed WebUI (index.html, bundle.js.gz, bundle.css.gz)
├── generated/                     # 🔄 5 generated C++ headers
├── docs/                          # Architecture documentation
├── project.json                   # Active modules list
└── CMakeLists.txt                 # Auto-runs generate_ui.py before build
```

---

## Quick Start

### Prerequisites

- [ESP-IDF v5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/get-started/)
- Python 3.8+ (for generator and tests)
- Node.js 18+ (for WebUI build)
- ESP32-WROOM-32 board (DevKit GPIO-direct) or KC868-A6 (I2C PCF8574 expander)

### Build & Flash

```bash
cd ModESP_v4

# Build (auto-runs generate_ui.py)
idf.py build

# Flash and monitor
idf.py -p COM15 flash monitor
```

### Build WebUI

```bash
cd webui
npm install
npm run build
npm run deploy   # → data/www/
```

### Run Tests

```bash
python -m pytest tools/tests/ -v          # 264 pytest tests
cd tests/host && cmake -B build && cmake --build build && ctest --test-dir build  # 90 C++ host tests
```

---

## HTTP API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/state` | GET | Full SharedState as JSON |
| `/api/ui` | GET | UI schema (generated from manifests) |
| `/api/settings` | POST | Change writable state keys (with validation) |
| `/api/board` | GET | Board configuration |
| `/api/bindings` | GET/POST | Driver bindings (POST saves + restart) |
| `/api/modules` | GET | Module list and status |
| `/api/mqtt` | GET/POST | MQTT config and status |
| `/api/wifi` | POST | WiFi credentials |
| `/api/wifi/scan` | GET | WiFi scan results |
| `/api/ota` | GET/POST | Firmware info / OTA upload |
| `/api/onewire/scan` | GET | Scan OneWire bus (SEARCH_ROM) |
| `/api/log` | GET | DataLogger: streaming chunked JSON (?hours=24) |
| `/api/log/summary` | GET | DataLogger: record counts and flash usage |
| `/api/wifi/ap` | GET/POST | WiFi AP mode settings |
| `/api/time` | GET/POST | Current time / NTP configuration |
| `/api/factory-reset` | POST | Factory reset (clear NVS + restart) |
| `/api/restart` | POST | Restart ESP32 |
| `/ws` | WS | Real-time state broadcast (max 3 clients, 3s interval, 20s heartbeat) |

---

## Current Status

**Phase 17 (Compressor Safety) + WebUI Premium Redesign R1 complete.** Milestone M3 "Production Ready" achieved. The following is fully operational:

- 5 business modules: Equipment Manager, Thermostat v2, Defrost (7-phase), Protection (10 alarms + CompressorTracker), DataLogger (6-ch)
- 6 drivers: DS18B20 (OneWire), Relay (GPIO), Digital Input (GPIO), NTC (ADC), PCF8574 Relay (I2C), PCF8574 Input (I2C)
- 2 boards: ESP32-DevKit (GPIO direct), KC868-A6 (I2C PCF8574 expander)
- 6-channel temperature logging with event history, SVG chart, CSV export
- Night Setback (4 modes), post-defrost alarm suppression, display during defrost
- Progressive feature disclosure with runtime UI visibility (visible_when, requires_state)
- Compressor Safety: short/rapid cycling, continuous run, pulldown failure, rate-of-change alarms + motor hours tracking
- Svelte WebUI: bento-card dashboard, GroupAccordion responsive, 41 Svelte files, light/dark theme, i18n UA/EN (76KB gzipped)
- MQTT with TLS, OTA with rollback, auto-persist settings to NVS
- WiFi STA + AP fallback (country code UA, STA watchdog)
- 61 STATE_META entries, 48 MQTT pub, 60 MQTT sub
- 264 pytest tests + 90 C++ host tests green
- Free heap after boot: **176 KB** / 240 KB

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
| WebUI | Svelte 4, Rollup, Lucide icons, i18n UA/EN, GroupAccordion (responsive) |
| Code generation | Python 3 (manifest → 5 artifacts, ~1644 lines) |
| Testing | pytest (264 tests) + doctest (90 C++ host tests) |

---

## License

This project is licensed under the **GNU General Public License v3.0**.

---

## Українською

ModESP v4 — модульний фреймворк прошивки для промислових ESP32-контролерів
холодильного обладнання. Замінює дорогі контролери Danfoss/Dixell дешевим
модулем ESP32 з професійною архітектурою.

Уся внутрішня документація та коментарі в коді — **українською мовою**.
