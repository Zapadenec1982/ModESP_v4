# ModESP v4

**Modular ESP32 firmware framework for industrial refrigeration controllers**

![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.5-blue)
![C++17](https://img.shields.io/badge/C%2B%2B-17-orange)
![Tests](https://img.shields.io/badge/Tests-491%20passed-brightgreen)
![License](https://img.shields.io/badge/License-PolyForm%20Noncommercial-blue)

Manifest-driven firmware that generates UI, state metadata, MQTT topics, and feature flags from JSON — zero manual sync between firmware, WebUI, and cloud.

<!-- TODO: ![WebUI Screenshot](docs/img/dashboard.png) -->

---

## Key Metrics

| Metric | Value |
|--------|-------|
| State keys | 126 (63 metadata, 50 MQTT pub, 62 MQTT sub) |
| Modules | 5 (equipment, thermostat, defrost, protection, datalogger) |
| Drivers | 6 (DS18B20, NTC, relay, digital input, PCF8574 relay/input) |
| HTTP endpoints | 23 REST + WebSocket + OTA upload |
| Tests | 491 (310 pytest + 181 C++ host / 418 assertions) |
| WebUI | 80 KB gzipped, Svelte 4, dark/light theme, UA/EN |
| Firmware binary | ~1.2 MB, free heap 77–90 KB operational |
| Target | ESP32-WROOM-32, 4 MB flash, ESP-IDF v5.5 |

> **Full feature list:** [docs/FEATURES.md](docs/FEATURES.md) | [Українською](docs/FEATURES_UA.md)

---

## How It Works

```
 module manifests ──┐
 driver manifests ──┼──▶  generate_ui.py  ──▶  ui.json        (WebUI schema)
 board.json        ──┤                     ──▶  state_meta.h   (C++ metadata)
 bindings.json     ──┘                     ──▶  mqtt_topics.h  (pub/sub arrays)
                                           ──▶  features_config.h
                                           ──▶  display_screens.h
```

Add a module manifest → rebuild → new parameters appear in WebUI, MQTT, state engine, and persistence automatically. No manual wiring.

### Equipment Arbitration

```
  Thermostat          Defrost           Protection
      │                  │                  │
      │ req.compressor   │ req.defrost      │ lockout / block
      ▼                  ▼                  ▼
  ┌──────────────────────────────────────────────┐
  │           Equipment Manager                   │
  │  Priority: Protection > Defrost > Thermostat  │
  │  Interlocks: defrost ↔ compressor             │
  └──────────────────────────────────────────────┘
      │           │           │           │
    Relay       Relay       Relay       Relay
   (comp)    (evap_fan)  (cond_fan)   (defrost)
```

Business modules publish requests to SharedState. Equipment Manager arbitrates and drives hardware. Modules never touch GPIO directly.

---

## Refrigeration Features

**Thermostat** — 8-state FSM, asymmetric differential, night setback (4 modes), safety run on sensor failure, configurable min on/off times

**Defrost** — 7-phase FSM (pre-drip → defrost → drip → equalize → fan delay → stabilize → normal), 3 types (natural, electric, hot-gas), 4 initiations (timer, demand, manual, digital input)

**Protection** — 10 independent alarm monitors with 2-level escalation:

| Monitor | Trigger | Escalation |
|---------|---------|------------|
| High/Low temp | Threshold + delay | Warning → lockout |
| Sensor failure (×2) | No reading | Immediate |
| Door open | Contact + delay | Warning |
| Short/Rapid cycle | Frequency detection | Compressor blocked |
| Continuous run | Max runtime exceeded | Forced off → lockout |
| Pulldown failure | Min drop not met | Warning |
| Rate of change | °C/min threshold | Warning |

**DataLogger** — 6 temperature channels, 18 event types, LittleFS storage with rotation, streaming JSON API, SVG chart, CSV export

---

## Connectivity

- **WiFi** — STA + AP fallback, AP→STA periodic probe (30s–5min backoff, 50 KB heap guard), mDNS, STA watchdog
- **MQTT** — TLS (port 8883), delta-publish (only changed values), heartbeat JSON every 30s, LWT, tenant-aware topic prefix
- **HTTP** — 23 REST endpoints: state, settings, bindings, WiFi, OTA, logs, backup/restore, factory reset
- **WebSocket** — real-time state broadcast, max 3 clients, delta updates
- **Cloud** — [ModESP Cloud](https://github.com/Zapadenec1982/ModESP_Cloud) integration: auto-discovery, signed OTA, HACCP compliance ([details](docs/CLOUD_INTEGRATION.md))

---

## Web Interface

Svelte 4 SPA — 80 KB gzipped, served from ESP32 LittleFS.

- Bento-card dashboard with grouped parameters
- Real-time updates via WebSocket (no polling)
- Light / dark theme, responsive layout
- UA / EN localization
- Progressive disclosure — UI shows only settings for connected hardware
- GroupAccordion with expand/collapse, parameter validation

<!-- TODO: Screenshots -->

---

## Quick Start

```bash
# Prerequisites: ESP-IDF v5.5, Python 3.8+, Node.js 18+

# Build firmware (auto-generates code from manifests)
idf.py build

# Flash to ESP32
idf.py -p /dev/ttyUSB0 flash monitor

# Build WebUI (optional — pre-built bundle included)
cd webui && npm install && npm run build && npm run deploy
```

---

## Project Structure

```
components/
├── modesp_core/        # BaseModule, ModuleManager, SharedState, types
├── modesp_services/    # Error, Watchdog, Config, Persist, Logger, SystemMonitor
├── modesp_hal/         # HAL, DriverManager, driver interfaces
├── modesp_net/         # WiFi, HTTP (23 endpoints), WebSocket
├── modesp_mqtt/        # MQTT client with TLS, delta-publish, HA discovery
└── modesp_json/        # JSON serialization (jsmn-based)
modules/
├── equipment/          # HAL owner, arbitration, interlocks
├── thermostat/         # 8-state FSM, fan control, night setback
├── defrost/            # 7-phase FSM, 3 types, 4 initiations
├── protection/         # 10 monitors, CompressorTracker, motor hours
└── datalogger/         # 6-ch temperature + 18 event types (LittleFS)
drivers/
├── ds18b20/            # Dallas OneWire temperature sensor
├── ntc/                # NTC thermistor via ADC (B-parameter)
├── relay/              # GPIO relay with min on/off protection
├── digital_input/      # GPIO digital input (door contact)
├── pcf8574_relay/      # I2C PCF8574 relay (KC868-A6)
└── pcf8574_input/      # I2C PCF8574 input (KC868-A6)
tools/
├── generate_ui.py      # Manifest → 5 artifacts (~1644 lines)
└── tests/              # 310 pytest tests
webui/                  # Svelte 4 source (24 widget components)
tests/host/             # 181 C++ doctest tests (418 assertions)
```

---

## Tech Stack

| Layer | Technology |
|-------|-----------|
| MCU | ESP32-WROOM-32, 4 MB flash |
| Framework | ESP-IDF v5.5, FreeRTOS |
| Language | C++17, zero heap allocation (ETL containers) |
| JSON | jsmn (header-only, zero-alloc parser) |
| Storage | LittleFS (960 KB data partition) |
| WebUI | Svelte 4, Rollup, Lucide icons |
| Code gen | Python 3 (manifest → 5 C++ headers + UI JSON) |
| Testing | pytest 310 + doctest 181 (418 assertions) |
| Cloud | [ModESP Cloud](https://github.com/Zapadenec1982/ModESP_Cloud) (Node.js, PostgreSQL, Mosquitto) |

---

## Documentation

| Document | Description |
|----------|-------------|
| [FEATURES.md](docs/FEATURES.md) | Complete feature overview (EN) |
| [FEATURES_UA.md](docs/FEATURES_UA.md) | Повний огляд можливостей (UA) |
| [ARCHITECTURE.md](ARCHITECTURE.md) | System architecture deep-dive |
| [CLOUD_INTEGRATION.md](docs/CLOUD_INTEGRATION.md) | ModESP Cloud integration guide |
| [docs/](docs/) | 12 technical documents (architecture, modules, manifests, WebUI) |

---

## License

[**PolyForm Noncommercial License 1.0.0**](LICENSE)

Free for personal and non-commercial use. Commercial licensing available — contact via [GitHub](https://github.com/Zapadenec1982) or [LinkedIn](https://www.linkedin.com/in/yurii-tepliuk/).

---

**Yurii Tepliuk** — Embedded Systems Engineer, Ukraine
ESP32 · ESP-IDF · FreeRTOS · C++17 · Full-stack IoT

---

*Українською:* ModESP v4 — модульний фреймворк для промислових ESP32-контролерів холодильного обладнання. Manifest-driven архітектура, Svelte WebUI, MQTT+TLS, OTA з відкатом. [Документація українською](docs/FEATURES_UA.md).
