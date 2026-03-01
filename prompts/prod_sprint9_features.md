# Production Sprint 9 — Advanced Features (5-6 сесій)

## Контекст

Security, display, testing, CI/CD done. Тепер advanced features для конкурентоспроможності.

## Сесія 9a: Modbus RTU/TCP

### Задачі
- [ ] Створити `components/modesp_modbus/` component
- [ ] Modbus RTU slave через UART (RS-485 transceiver)
- [ ] Register map: state keys → Holding/Input registers
  - Input Registers (read-only): temperatures, uptime, alarm status
  - Holding Registers (read-write): setpoint, alarm limits, defrost params
  - Coils: relay states (read-only)
  - Discrete Inputs: alarm flags, compressor state
- [ ] Modbus TCP slave (WiFi, port 502)
- [ ] Board.json: uart_buses configuration
- [ ] Test: ModScan or similar tool → read/write registers

## Сесія 9b: Home Assistant MQTT Auto-Discovery

### Задачі
- [ ] mDNS: `modesp-XXXX.local` service announcement
- [ ] MQTT Discovery topics: `homeassistant/sensor/modesp_XXXX/air_temp/config`
- [ ] Entity types: sensor (temps), binary_sensor (alarms), switch (defrost manual), climate (thermostat)
- [ ] Device registry: name, manufacturer, firmware version, model
- [ ] Retain flag on discovery messages
- [ ] Auto-refresh discovery on reconnect

## Сесія 9c: Scheduled Defrost (AUDIT-032)

### Задачі
- [ ] defrost.schedule_enabled state key (persist, bool)
- [ ] defrost.schedule_times state key (persist, string "02:00,14:00")
- [ ] Max 4 defrost times per day
- [ ] Потребує SNTP (вже є) — skip якщо time not synced
- [ ] WebUI: time picker для кожного слоту
- [ ] Priority: scheduled > timer-based (якщо обидва enabled)

## Сесія 9d: Pressure Switches (AUDIT-031)

### Задачі
- [ ] hp_switch, lp_switch ролі в Equipment
- [ ] Protection: 2 нові монітори (High Pressure, Low Pressure)
- [ ] HP alarm: instant (0ms delay) → компресор OFF
- [ ] LP alarm: configurable delay → компресор OFF
- [ ] Auto-clear при зникненні сигналу (або manual reset)

## Сесія 9e: PID Controller

### Задачі
- [ ] thermostat.control_mode state key: 0=on_off, 1=pid
- [ ] PID: Kp, Ki, Kd parameters (persist)
- [ ] Output: 0-100% duty cycle для компресора (cycle time configurable)
- [ ] Anti-windup на integrator
- [ ] Bumpless transfer при switch on_off ↔ PID
- [ ] Host tests для PID algorithm

## Сесія 9f: Adaptive Defrost

### Задачі
- [ ] Аналіз T_evap тренду: якщо evap чистий (T_evap > threshold) → skip defrost
- [ ] Лічильник пропусків: max 3 consecutive skips → force defrost
- [ ] Статистика: середня тривалість defrost → adaptive interval
- [ ] defrost.adaptive_enabled state key (persist)
