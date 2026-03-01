# Sprint 1 / Сесія 1.1a — Delta-Only WS Broadcasts

## Контекст

Кореневі причини memory leak (зараз виправлено костилями):
- WS broadcast серіалізує ВСІ ~97 state keys (~3.5KB) кожні 3с
- `malloc(sizeof(AsyncSendCtx) + len)` × 3 клієнти = 10.5KB heap churn кожні 3с
- **Костиль 1:** Heap guard 40KB в ws_service.cpp:361 — skip broadcast якщо heap < 40KB
- **Костиль 2:** Broadcast interval 3000ms (було 1000ms) — зменшує частоту malloc

**Ціль:** Серіалізувати тільки змінені ключі (delta). Типовий delta: 3-5 ключів = ~200 байт.

## Задачі

### 1. SharedState — Change Tracking

Файл: `components/modesp_core/include/modesp/shared_state.h` + `.cpp`

- Додати `etl::bitset<MODESP_MAX_STATE_ENTRIES>` changed_keys_ (member)
- При `set()` — якщо значення змінилось, позначити відповідний біт
- Додати параметр `bool track_change = true` до `set()`:
  - Internal timers (comp_on_time_ms, phase_timer тощо) — `set(key, val, false)`
  - Це вирішує BUG-025: таймери не тригерять version bumps та broadcasts
- Новий метод `for_each_changed(callback)` — ітерує тільки змінені ключі
- `mark_all_changed()` — для initial full-state send новому клієнту
- `clear_changed()` — після broadcast
- Thread-safe: changed_keys_ під тим же mutex що й entries_

**Увага:** SharedState використовує `etl::unordered_map` — індексація за position в map.
Потрібна mapping від key до bit index. Варіант: використовувати hash від ключа по модулю
розміру bitset, або окремий `etl::vector<StateKey, N>` changed_list.

Альтернативний підхід (простіший):
```cpp
etl::vector<StateKey, 32> changed_keys_;  // Max 32 changes between broadcasts
void set(...) { if (value_changed) { if (!changed_keys_.full()) changed_keys_.push_back(key); } }
```

### 2. WsService — Delta Serialization

Файл: `components/modesp_net/src/ws_service.cpp`

- `broadcast_state()` → перевірити `changed_keys_` замість full state
- Якщо `changed_keys_` пустий — skip broadcast (нічого не змінилось)
- Серіалізація: тільки змінені ключі → `{"thermostat.temperature":-18.5,"system.uptime":3600}`
- **Initial client:** при новому WS підключенні — `mark_all_changed()` → повний state
- Після broadcast — `clear_changed()`

### 3. WebUI — Merge Partial Updates

Файл: `webui/src/stores/state.js`

- Перевірити що `state.update(s => ({...s, ...data}))` працює з partial JSON
- Поточний код вже робить merge — верифікувати, не міняти якщо працює
- Тестувати: WS отримує `{"thermostat.temperature":-18.5}` → state зберігає решту ключів

### 4. Module Updates — track_change=false для таймерів

Файли модулів (thermostat, defrost, equipment):
- Знайти всі `state_set()` calls для internal timers
- Додати `false` як третій параметр (track_change)
- Типові кандидати:
  - `thermostat.comp_on_time_ms`
  - `defrost.phase_timer`
  - `system.uptime`
  - `system.loop_time_us`
  - будь-які counters що змінюються кожну ітерацію

## Файли

| Файл | Дія |
|------|-----|
| `components/modesp_core/include/modesp/shared_state.h` | РЕДАГУВАТИ |
| `components/modesp_core/src/shared_state.cpp` | РЕДАГУВАТИ |
| `components/modesp_net/src/ws_service.cpp` | РЕДАГУВАТИ |
| `webui/src/stores/state.js` | ВЕРИФІКУВАТИ (можливо без змін) |
| `modules/thermostat/src/thermostat_module.cpp` | РЕДАГУВАТИ (track_change) |
| `modules/defrost/src/defrost_module.cpp` | РЕДАГУВАТИ (track_change) |
| `modules/equipment/src/equipment_module.cpp` | РЕДАГУВАТИ (track_change) |
| `main/main.cpp` | РЕДАГУВАТИ (system.uptime track_change) |

## Критерії завершення

- [ ] `idf.py build` без помилок та warnings
- [ ] Delta broadcast працює: WS JSON містить тільки змінені ключі
- [ ] Новий клієнт отримує повний state при підключенні
- [ ] Таймерні ключі НЕ тригерять broadcasts (track_change=false)
- [ ] WebUI коректно merge'ить partial updates
- [ ] `pytest tools/tests/ -v` — всі тести зелені
- [ ] Host C++ tests pass

## Після завершення

```bash
idf.py build
cd tools && python -m pytest tests/ -v
git add components/modesp_core/ components/modesp_net/src/ws_service.cpp \
  modules/*/src/*.cpp main/main.cpp
git commit -m "feat(ws): delta-only broadcasts — SharedState change tracking, 94% payload reduction

- SharedState: changed_keys_ tracking, for_each_changed(), track_change parameter
- WsService: serialize only changed keys, full state for new clients
- Modules: track_change=false for internal timers (fixes BUG-025 properly)
- Typical delta: ~200 bytes vs ~3500 bytes full state"
git push origin main
```
