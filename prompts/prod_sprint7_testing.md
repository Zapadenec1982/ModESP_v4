# Production Sprint 7 — Extended Testing (2 сесії)

## Контекст

Equipment та DataLogger модулі мають 0 host tests. Потрібно покрити перед Production.

## Сесія 7a: Equipment + DataLogger Host Tests

### Equipment Module Tests
- [ ] Arbitration: Protection request > Defrost request > Thermostat request
- [ ] Relay interlocks: defrost_relay та compressor НІКОЛИ одночасно ON
- [ ] Anti-short-cycle: compressor OFF мінімум 180с, ON мінімум 120с
- [ ] EMA filter: точність (alpha = 1/(coeff+1)), convergence
- [ ] Night input: equipment.night_input state key
- [ ] has_* keys: публікація equipment.has_evap_temp etc.

### DataLogger Module Tests
- [ ] TempRecord serialization: 16 bytes, ch[6] array
- [ ] TEMP_NO_DATA sentinel: INT16_MIN для disabled channels
- [ ] ChannelDef: enable_key + has_key logic
- [ ] Record append + count
- [ ] Auto-migration: 8/12 byte records → видалення при boot
- [ ] Event types: 10 типів (compressor on/off, defrost start/end, etc.)

### Файли
- `tests/host/test_equipment.cpp` — СТВОРИТИ
- `tests/host/test_datalogger.cpp` — СТВОРИТИ
- `tests/host/CMakeLists.txt` — ОНОВИТИ

## Сесія 7b: WebUI Integration Tests

### Задачі
- [ ] Setup: Playwright або Cypress against mock API server
- [ ] Mock API: Python Flask/FastAPI server що імітує ESP32 endpoints
- [ ] Tests:
  - Dashboard renders temperature + pills
  - Settings change → debounce → POST → success feedback
  - Login modal on 401
  - Connection overlay on WS disconnect
  - °C/°F toggle
  - Chart renders with sample data
- [ ] Visual regression: screenshot comparison

### Критерії завершення
- [ ] 120+ host C++ tests (was 90)
- [ ] WebUI integration tests pass
- [ ] No regression in existing 264 pytest + 90 doctest
