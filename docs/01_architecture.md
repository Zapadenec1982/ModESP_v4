# ModESP v4 — Архітектура

## Філософія

ModESP v4 — модульна прошивка для промислових холодильників на ESP32.

**Ключові принципи:**
- Zero heap allocation в hot path (ETL замість STL)
- Layered architecture: Core → Services → HAL → Drivers → Modules
- Кожен модуль — ізольований, тестований, замінний
- JSON тільки на cold path (конфігурація, WebSocket, діагностика)
- Передбачуване використання RAM: відоме при компіляції

## Шари системи

```
┌─────────────────────────────────────────────┐
│                   main.cpp                   │  Збирає все разом
├──────────┬──────────┬───────────┬────────────┤
│ modules/ │ drivers/ │ modesp_   │ modesp_    │
│ (бізнес) │ (HW)     │ net (опц.)│ json(cold) │
├──────────┴──────────┴───────────┴────────────┤
│              modesp_services                  │  Системні сервіси
├──────────────────────────────────────────────┤
│                modesp_hal                     │  Hardware abstraction
├──────────────────────────────────────────────┤
│                modesp_core                    │  Ядро: BaseModule,
│                  + ETL                        │  SharedState, MessageBus
├──────────────────────────────────────────────┤
│            ESP-IDF / FreeRTOS                 │
└──────────────────────────────────────────────┘
```

## Залежності між компонентами

```
                     ┌─────────────┐
                     │    main/    │  Точка входу
                     └──────┬──────┘
                            │ збирає все разом
       ┌──────────┬─────────┼─────────┬──────────┐
       │          │         │         │          │
┌──────▼───┐ ┌───▼────┐ ┌──▼───┐ ┌───▼───┐ ┌───▼────┐
│ modules/ │ │drivers/│ │config│ │modesp │ │modesp  │
│(бізнес)  │ │(HW)   │ │JSON  │ │_net   │ │_json   │
└──────┬───┘ └───┬────┘ │files │ │(опц.) │ │(cold)  │
       │         │      └──────┘ └───┬───┘ └───┬────┘
       │         │                   │         │
       └────┬────┘                   │         │
            │                        │         │
     ┌──────▼──────┐                 │         │
     │ modesp_hal/ │                 │         │
     │ (hardware)  │                 │         │
     └──────┬──────┘                 │         │
            │                        │         │
     ┌──────▼──────────────┐         │         │
     │ modesp_services/    │◄────────┘         │
     │ config, error, log, │                   │
     │ watchdog, persist,  │                   │
     │ system_monitor      │                   │
     └──────┬──────────────┘                   │
            │                                  │
            └─────────────┬────────────────────┘
                          │
                   ┌──────▼──────┐
                   │ modesp_core │  ← Залежить тільки від
                   │   + ETL     │     ETL та FreeRTOS
                   └─────────────┘
```

**Правила залежностей:**
- `modesp_core` → тільки ETL + ESP-IDF (FreeRTOS)
- `modesp_services` → modesp_core + NVS/LittleFS
- `modesp_hal` → modesp_core
- `modesp_net` → modesp_core + modesp_services
- `modesp_json` → modesp_core + nlohmann/json
- `drivers/` → modesp_core + modesp_hal
- `modules/` → modesp_core + drivers (за потребою)
- `main/` → все

## Структура проекту

```
ModESP_v4/
├── docs/                             # Документація
├── components/
│   ├── etl/                          # ETL (git submodule)
│   ├── modesp_core/                  # Ядро (~700 рядків)
│   │   ├── include/modesp/
│   │   │   ├── core.h                # Головний include
│   │   │   ├── types.h               # StateKey, StateValue, msg_id
│   │   │   ├── message_types.h       # Базові системні повідомлення
│   │   │   ├── base_module.h         # Базовий клас модуля
│   │   │   ├── module_manager.h      # Реєстрація та lifecycle
│   │   │   ├── shared_state.h        # Key-value сховище стану
│   │   │   └── app.h                 # Application lifecycle
│   │   └── src/
│   ├── modesp_services/              # Системні сервіси (~990 рядків)
│   │   ├── include/modesp/services/
│   │   │   ├── error_service.h       # Помилки + Safe Mode
│   │   │   ├── watchdog_service.h    # Моніторинг здоров'я модулів
│   │   │   ├── config_service.h      # Завантаження/збереження конфіг NVS
│   │   │   ├── persist_service.h     # Автозбереження SharedState
│   │   │   ├── logger_service.h      # Ring buffer логів
│   │   │   └── system_monitor.h      # RAM, uptime, boot reason
│   │   └── src/
│   ├── modesp_hal/                   # Hardware abstraction
│   │   ├── include/modesp/hal/
│   │   │   ├── gpio.h, onewire.h, i2c.h, adc.h, board.h
│   │   └── src/
│   ├── modesp_net/                   # Мережа (опціонально)
│   │   ├── include/modesp/net/
│   │   │   ├── wifi_service.h, web_server.h, mqtt_service.h
│   │   │   ├── ota_service.h, ntp_service.h
│   │   └── src/
│   └── modesp_json/                  # JSON bridge (cold path)
│       ├── include/modesp/json/
│       │   ├── state_serializer.h, config_loader.h, rpc_handler.h
│       └── src/
├── drivers/                          # Драйвери пристроїв
│   ├── ds18b20/, ntc/, relay/, digital_input/
├── modules/                          # Бізнес-логіка
│   ├── equipment/, thermostat/, protection/, defrost/, datalogger/
├── main/                             # Точка входу
│   ├── main.cpp
├── data/                             # Runtime конфігурація + WebUI
│   ├── board.json, bindings.json, ui.json, www/
├── CMakeLists.txt
├── sdkconfig
└── partitions.csv
```

## Потік даних

```
DS18B20 ─publish(SensorReading)─► Message Bus ─► Thermostat.on_message()
                                                        │
                                                   control_loop()
                                                        │
                                       publish(ActuatorCommand)
                                                        │
                                                   Relay Driver
```

Паралельно: SharedState → JSON Bridge → WebSocket / MQTT (cold path, 1 Hz)

## Розміри (оцінка)

| Компонент | Рядків | RAM |
|-----------|--------|-----|
| modesp_core | ~720 | ~6-7 KB |
| modesp_services | ~990 | ~9 KB |
| **Разом core+services** | **~1700** | **~15-16 KB** |

ESP32 має 520 KB RAM. Ядро + сервіси займають ~3% від доступної пам'яті.
