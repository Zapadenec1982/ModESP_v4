# ModESP v4 — Phase 5b: UI Generation from Module Manifests

## Philosophy

Each module in ModESP v4 describes itself through a `manifest.json` file.
A Python build-time script reads all active module manifests and generates
every artifact the firmware and web UI need — zero manual wiring, zero
runtime overhead.

**Single Source of Truth** — the manifest is the only place you define:
- State keys (types, ranges, defaults, persistence)
- Web UI layout (pages, cards, widgets)
- MQTT topics (publish/subscribe)
- Display/LCD screens (main values, menu items)

## What Gets Generated

| # | Output File | Purpose |
|---|-------------|---------|
| 1 | `data/ui.json` | Merged UI schema served at `GET /api/ui` |
| 2 | `data/www/index.html` | Self-contained web interface (inline CSS/JS) |
| 3 | `generated/state_meta.h` | `constexpr` metadata for API validation |
| 4 | `generated/mqtt_topics.h` | `constexpr` MQTT topic string arrays |
| 5 | `generated/display_screens.h` | `constexpr` display/LCD data |

All C++ headers are `constexpr` — no heap, no runtime cost, linker-resolved.

## Pipeline

```
project.json           ┐
modules/*/manifest.json┤
                       ├─→  tools/generate_ui.py
                       │
                       ├─→  data/ui.json
                       ├─→  data/www/index.html
                       ├─→  generated/state_meta.h
                       ├─→  generated/mqtt_topics.h
                       └─→  generated/display_screens.h
```

The generator runs **before** the IDF build via `execute_process()` in the
root `CMakeLists.txt`. If validation fails, the build stops with a clear
error message.

## Manifest Format

Each module has `modules/<name>/manifest.json`:

```json
{
  "module": "thermostat",
  "description": "On/off thermostat with hysteresis",

  "requires": [
    {"role": "chamber_temp", "type": "sensor",   "driver": "ds18b20"},
    {"role": "compressor",   "type": "actuator", "driver": "relay"}
  ],

  "state": {
    "thermostat.temperature": {
      "type": "float",
      "unit": "°C",
      "access": "read",
      "description": "Current chamber temperature"
    },
    "thermostat.setpoint": {
      "type": "float",
      "unit": "°C",
      "access": "readwrite",
      "min": -35.0,
      "max": 0.0,
      "step": 0.5,
      "default": -18.0,
      "persist": true,
      "description": "Temperature setpoint"
    }
  },

  "ui": {
    "page": "Cold Room",
    "page_id": "thermostat",
    "icon": "snowflake",
    "order": 1,
    "cards": [
      {
        "title": "Temperature",
        "widgets": [
          {
            "key": "thermostat.temperature",
            "widget": "gauge",
            "size": "large",
            "color_zones": [
              {"to": -22, "color": "#3b82f6"},
              {"to": -15, "color": "#22c55e"},
              {"to":  10, "color": "#ef4444"}
            ]
          },
          {
            "key": "thermostat.setpoint",
            "widget": "slider"
          }
        ]
      }
    ]
  },

  "mqtt": {
    "publish":   ["thermostat.temperature", "thermostat.compressor"],
    "subscribe": ["thermostat.setpoint", "thermostat.hysteresis"]
  },

  "display": {
    "main_value": {
      "key": "thermostat.temperature",
      "format": "%.1f°C"
    },
    "menu_items": [
      {"label": "Setpoint",   "key": "thermostat.setpoint"},
      {"label": "Hysteresis", "key": "thermostat.hysteresis"}
    ]
  }
}
```

### State Key Properties

| Field | Required | Description |
|-------|----------|-------------|
| `type` | Yes | `float`, `int`, `bool`, or `string` |
| `access` | Yes | `read` or `readwrite` |
| `unit` | No | Display unit (e.g. `°C`, `%`) |
| `min` | Req. for readwrite float/int | Minimum allowed value |
| `max` | Req. for readwrite float/int | Maximum allowed value |
| `step` | No | Adjustment step (default: 1.0 for int, 0.1 for float) |
| `default` | No | Default value on first boot |
| `persist` | No | If `true`, value is saved to NVS |
| `enum` | No | Allowed string values for `string` type |
| `description` | No | Human-readable description |

### Widget Types

| Widget | Compatible Types | Description |
|--------|-----------------|-------------|
| `gauge` | float, int | Circular/linear gauge display |
| `slider` | float, int | Adjustable slider control |
| `number_input` | float, int | Numeric input with +/- buttons |
| `indicator` | bool | On/off status indicator |
| `toggle` | bool | On/off toggle switch |
| `status_text` | string | Status text display |
| `value` | float, int, bool, string | Universal read-only value display |
| `chart` | float | Time-series chart |

### Widget Options

Widgets accept extra properties depending on their type:

- **gauge**: `size` (small/large), `color_zones` (array of `{to, color}`)
- **slider**: inherits `min`, `max`, `step` from state key
- **number_input**: inherits `min`, `max`, `step` from state key
- **indicator**: `on_label`, `off_label`, `on_color`, `off_color`
- **toggle**: `on_label`, `off_label`
- **status_text**: no extra properties
- **value**: `format` (printf-style format string)

### Card Options

| Field | Required | Description |
|-------|----------|-------------|
| `title` | Yes | Card title |
| `widgets` | Yes | Array of widget objects |
| `group` | No | Logical group (e.g. `settings`) |
| `collapsible` | No | If `true`, card can be collapsed |

## Generated File Formats

### state_meta.h

Contains metadata only for **readwrite** state keys, used by the API
validation layer (`handle_post_settings`):

```cpp
namespace modesp::meta {

struct StateMeta {
    const char* key;
    float min_val;
    float max_val;
    float step;
};

static constexpr StateMeta STATE_META[] = {
    {"thermostat.setpoint", -35.0f, 0.0f, 0.5f},
    {"thermostat.hysteresis", 0.5f, 5.0f, 0.5f},
};
static constexpr size_t STATE_META_COUNT = 2;

// Lookup helper
inline const StateMeta* find_state_meta(const char* key) { ... }

} // namespace modesp::meta
```

### mqtt_topics.h

```cpp
namespace modesp::meta {

static constexpr const char* MQTT_PUBLISH[] = {
    "thermostat.temperature",
    "thermostat.compressor",
    "thermostat.state",
};
static constexpr size_t MQTT_PUBLISH_COUNT = 3;

static constexpr const char* MQTT_SUBSCRIBE[] = {
    "thermostat.setpoint",
    "thermostat.hysteresis",
};
static constexpr size_t MQTT_SUBSCRIBE_COUNT = 2;

} // namespace modesp::meta
```

### display_screens.h

```cpp
namespace modesp::meta {

struct MainValueEntry {
    const char* key;
    const char* format;
};

struct MenuItemEntry {
    const char* label;
    const char* key;
};

static constexpr MainValueEntry MAIN_VALUES[] = {
    {"thermostat.temperature", "%.1f\u00b0C"},
};
static constexpr size_t MAIN_VALUE_COUNT = 1;

static constexpr MenuItemEntry MENU_ITEMS[] = {
    {"Setpoint", "thermostat.setpoint"},
    {"Hysteresis", "thermostat.hysteresis"},
};
static constexpr size_t MENU_ITEM_COUNT = 2;

} // namespace modesp::meta
```

### ui.json

Merged schema with system pages (dashboard, network, system) + module pages:

```json
{
  "device": "ModESP Controller",
  "pages": [ ... ],
  "state_meta": {
    "thermostat.temperature": {"type":"float","unit":"°C","access":"read"},
    ...
  }
}
```

## System Pages

The generator adds three built-in pages (not from manifests):

1. **Dashboard** (`page_id: "dashboard"`, icon: `home`, order: 0)
   - Auto-generated overview of all modules
2. **Network** (`page_id: "network"`, icon: `wifi`, order: 90)
   - WiFi settings, connection status
3. **System** (`page_id: "system"`, icon: `settings`, order: 99)
   - Device info, restart, OTA

## Validation

The generator validates manifests at build time and fails fast with
clear error messages:

- **Required fields**: `module`, `state`, `ui`
- **State key naming**: must start with `<module>.`
- **readwrite keys**: must have `min` and `max` for float/int types
- **Widget compatibility**: widget type must match state key type
- **MQTT/display references**: all referenced keys must exist in `state`
- **Cross-module duplicates**: no two modules may define the same state key

## How to Add a New Module

1. Create `modules/<name>/manifest.json` following the format above
2. Ensure all state keys start with `<name>.`
3. Run `python tools/generate_ui.py` to validate and regenerate
4. The module's UI page, MQTT topics, and display screens appear automatically
5. Build with `idf.py build` — generator runs automatically via CMake

## CLI Usage

```bash
# Default (uses project root conventions)
python tools/generate_ui.py

# Explicit paths
python tools/generate_ui.py \
  --project project.json \
  --modules-dir modules \
  --output-data data \
  --output-gen generated

# Minified JSON output
python tools/generate_ui.py --minify
```

## CMake Integration

The root `CMakeLists.txt` runs the generator before the IDF build:

```cmake
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    execute_process(
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/tools/generate_ui.py"
            --project     "${CMAKE_SOURCE_DIR}/project.json"
            --modules-dir "${CMAKE_SOURCE_DIR}/modules"
            --output-data "${CMAKE_SOURCE_DIR}/data"
            --output-gen  "${CMAKE_SOURCE_DIR}/generated"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE GEN_RESULT
    )
    if(NOT GEN_RESULT EQUAL 0)
        message(FATAL_ERROR "generate_ui.py failed")
    endif()
endif()
```

The `main/CMakeLists.txt` includes the `generated/` directory:

```cmake
idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/generated"
    ...
)
```

## Constraints

- **Python 3.8+ stdlib only** — no pip dependencies
- **constexpr C++ headers** — zero heap allocation at runtime
- **No modification of module source code** — manifests are additive
- **Self-contained HTML** — single file, no CDN, works offline on ESP32
