# ModESP v4 — Phase 5a: WiFi + HTTP Server + REST API + WebSocket

## Мета документу

Архітектура мережевого стеку ModESP v4: WiFi підключення, HTTP сервер з REST API для читання/зміни стану системи, WebSocket для real-time оновлень. Це backend-частина — WebUI буде в Phase 5b.

**Попередні фази:**
- Phase 1: modesp_core (BaseModule, SharedState, ModuleManager, App) ✅
- Phase 2: modesp_services (ErrorService, WatchdogService, LoggerService, SystemMonitor) ✅
- Phase 3: Drivers + Thermostat (DS18B20, Relay, ThermostatModule) ✅
- Phase 4: HAL + ConfigService + DriverManager + LittleFS ✅
- **Phase 5a: WiFi + HTTP + REST API + WebSocket** ← ЦЕ

---

## Загальна архітектура мережі

```
┌─────────────────────────────────────────────────────────┐
│                      ESP32                               │
│                                                          │
│  ┌──────────────┐   ┌──────────────┐   ┌─────────────┐ │
│  │ WiFiService  │   │  HttpService │   │  WsService  │ │
│  │              │   │              │   │             │ │
│  │ • STA mode   │   │ • REST API   │   │ • Push      │ │
│  │ • SoftAP     │   │ • Static     │   │   updates   │ │
│  │   fallback   │   │   files      │   │ • 1 Hz      │ │
│  │ • NVS creds  │   │ • CORS       │   │             │ │
│  └──────┬───────┘   └──────┬───────┘   └──────┬──────┘ │
│         │                  │                   │        │
│         └──────────────────┴───────────────────┘        │
│                            │                             │
│                    esp_netif + lwIP                      │
└─────────────────────────────────────────────────────────┘
          │                  │                   │
          ▼                  ▼                   ▼
     WiFi Router        Browser HTTP        Browser WS
                        GET/POST            Real-time
```

---

## Компоненти Phase 5a

### 1. WiFiService — підключення до мережі

BaseModule з пріоритетом HIGH. Працює в двох режимах:

**STA mode (основний):** Підключається до WiFi мережі. SSID та password зберігаються в NVS. При втраті з'єднання — автоматичний reconnect з exponential backoff (2s → 4s → 8s → 16s → 32s max).

**SoftAP mode (provisioning):** Активується коли немає збережених WiFi credentials або після 10 невдалих спроб підключення. Створює точку доступу "ModESP-XXXX" (XXXX = останні 4 символи MAC). Без пароля для першого налаштування. HTTP сервер на 192.168.4.1 показує сторінку вводу WiFi credentials.

**Boot flow WiFi:**
```
WiFiService::on_init()
  │
  ├── Прочитати SSID/password з NVS
  │
  ├── Є credentials?
  │     ├── Так → esp_wifi_start() STA mode → connect()
  │     │         ├── Connected → publish IP в SharedState
  │     │         └── Failed (10 спроб) → switch to SoftAP
  │     │
  │     └── Ні → start SoftAP mode
  │
  └── Event handler:
        ├── WIFI_EVENT_STA_CONNECTED → log
        ├── IP_EVENT_STA_GOT_IP → save IP → SharedState("wifi.ip")
        ├── WIFI_EVENT_STA_DISCONNECTED → reconnect timer
        └── WIFI_EVENT_AP_STACONNECTED → log client connected
```

**SharedState ключі WiFi:**
```
wifi.mode       = "sta" | "ap" | "off"
wifi.ip         = "192.168.1.100"
wifi.ssid       = "MyNetwork"
wifi.rssi       = -65
wifi.connected  = true
wifi.ap_clients = 0
```

### 2. HttpService — HTTP сервер + REST API

BaseModule з пріоритетом LOW. Використовує ESP-IDF `esp_http_server`. Максимум 4 одночасних з'єднання.

**Статичні файли:** Serve-ить файли з LittleFS `/data/www/`. Підтримує gzip. Це для WebUI в Phase 5b.

**CORS headers:** Для розробки WebUI на PC. `Access-Control-Allow-Origin: *`.

### 3. REST API endpoints

#### GET /api/state — повний стан системи

```json
{
  "thermostat.temperature": 21.3,
  "thermostat.compressor": true,
  "thermostat.setpoint": -18.0,
  "wifi.ip": "192.168.1.100",
  "system.uptime": 3600,
  "system.heap_free": 267000
}
```

#### GET /api/board — конфігурація плати

```json
{
  "board": "dev_board_v1",
  "version": "1.0",
  "relays": [{"id": "relay_1", "gpio": 2, "active_high": true}],
  "onewire_buses": [{"id": "ow_1", "gpio": 15}]
}
```

#### GET /api/bindings — поточні bindings

```json
{
  "bindings": [
    {"hardware": "relay_1", "role": "compressor", "driver": "relay", "module": "thermostat"},
    {"hardware": "ow_1", "role": "chamber_temp", "driver": "ds18b20", "module": "thermostat"}
  ]
}
```

#### GET /api/modules — список активних модулів

```json
{
  "modules": [
    {"name": "thermostat", "state": "running", "priority": 2},
    {"name": "error", "state": "running", "priority": 0}
  ]
}
```

#### POST /api/settings — змінити налаштування модуля

```json
{"module": "thermostat", "key": "setpoint", "value": -20.0}
→ {"ok": true}
```

#### POST /api/wifi — зберегти WiFi credentials

```json
{"ssid": "MyNetwork", "password": "mypassword123"}
→ {"ok": true, "message": "Credentials saved. Restarting WiFi..."}
```

#### POST /api/restart — перезавантаження ESP

```json
→ {"ok": true, "message": "Restarting in 1 second..."}
```

### 4. WsService — WebSocket для real-time

Підключається на `/ws`. Кожну секунду перевіряє чи змінився SharedState (через version counter) і відправляє оновлення всім підключеним клієнтам.

Максимум 2 WebSocket клієнти. Heartbeat ping кожні 30 секунд.

---

## JSON серіалізація

Серіалізація SharedState → JSON виконується в стековий char буфер через snprintf. Без cJSON, мінімальний heap.

Розміри буферів: /api/state ~2KB, інші ~1KB.

---

## NVS Helper

Простий wrapper над ESP-IDF NVS: read/write для float, int32, bool, string. Namespace = module name, key = setting name.

---

## Зміни в існуючих компонентах

- **SharedState** — додано `version_` counter (інкрементується при кожному set)
- **ModuleManager** — додано `for_each_module()` для ітерації
- **main.cpp** — додано WiFi/HTTP/WS сервіси в boot flow

---

## Порядок boot з WiFi

```
STAGE 1: NVS init
STAGE 2: System services init (error, watchdog, config, logger, monitor)
STAGE 3: WiFi init → STA connect (або SoftAP)
STAGE 4: HAL init + DriverManager
STAGE 5: HTTP server + WebSocket handler
STAGE 6: Business modules (thermostat)
STAGE 7: Main loop (100 Hz)
```

WiFi та HTTP працюють в окремих FreeRTOS tasks, не блокують main loop.

---

## Тестування

```bash
curl http://192.168.1.100/api/state
curl http://192.168.1.100/api/modules
curl -X POST http://192.168.1.100/api/settings -d '{"module":"thermostat","key":"setpoint","value":-20.0}'
# або браузер: http://192.168.1.100/
```

---

## Що НЕ входить в Phase 5a

- Svelte WebUI (Phase 5b)
- POST /api/bindings (Phase 5b)
- OTA (Phase 6+)
- MQTT (Phase 6+)
- HTTPS/TLS, mDNS, HTTP auth (Phase 6+)
