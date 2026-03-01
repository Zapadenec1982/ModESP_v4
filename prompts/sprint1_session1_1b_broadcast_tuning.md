# Sprint 1 / Сесія 1.1b — Broadcast Tuning + Cleanup

## Контекст

Попередня сесія (1.1a) реалізувала delta broadcasts. Типовий payload ~200 байт.
Тепер прибираємо костилі та тюнимо параметри.

## Задачі

### 1. Знизити/прибрати Heap Guard

Файл: `components/modesp_net/src/ws_service.cpp`

- Рядок ~361: `if (esp_get_free_heap_size() < 40000)` → зменшити до 16000 або прибрати
- Рядок ~409: аналогічно для ping
- З delta broadcasts payload ~200 байт, AsyncSendCtx < 1KB — guard 40KB надмірний
- **Рекомендація:** залишити guard але знизити до 16KB як safety net

### 2. Зменшити Broadcast Interval

Файл: `components/modesp_net/include/modesp/net/ws_service.h`

- `BROADCAST_INTERVAL_MS` = 3000 → **1500**
- Delta payload малий (~200 байт) — можна частіше оновлювати UI
- UI latency покращується з 3с до 1.5с — відчутна різниця для користувача

### 3. Прибрати MQTT Timer Key Exclusion (BUG-025)

Якщо BUG-025 був вирішений через видалення timer keys з MQTT publish manifests:
- Повернути timer keys в маніфести (вони потрібні для MQTT monitoring)
- track_change=false в SharedState (з сесії 1.1a) вирішує проблему на рівні framework
- Верифікувати що timer keys НЕ тригерять WS broadcasts (delta only)
- Верифікувати що timer keys публікуються через MQTT (з MQTT publish interval)

### 4. Верифікація на залізі

Якщо є доступ до ESP32:
- Flash нову прошивку
- Моніторити heap через serial: `system.heap_free`, `system.heap_largest`
- Відкрити 3 WS клієнти (3 вкладки браузера)
- Спостерігати 1+ годину: heap повинен бути стабільним
- Перевірити що WS payload зменшився (browser DevTools → Network → WS messages)

### 5. Cleanup коментарів

- Прибрати коментарі "BUG-025 workaround" якщо є
- Прибрати коментарі "костиль" / "temporary" пов'язані з heap guard

## Файли

| Файл | Дія |
|------|-----|
| `components/modesp_net/src/ws_service.cpp` | РЕДАГУВАТИ (heap guard) |
| `components/modesp_net/include/modesp/net/ws_service.h` | РЕДАГУВАТИ (interval) |
| `modules/*/manifest.json` | МОЖЛИВО (повернути timer keys якщо видалені) |

## Критерії завершення

- [ ] `idf.py build` без помилок
- [ ] Heap guard знижено до 16KB (або прибрано)
- [ ] Broadcast interval = 1500ms
- [ ] BUG-025 workaround прибрано (timer keys через track_change=false)
- [ ] На залізі: heap стабільний 1+ годину з 3 WS клієнтами

## Після завершення

```bash
idf.py build
git add components/modesp_net/
git commit -m "fix(ws): cleanup memory workarounds — lower heap guard, faster broadcasts

- Heap guard: 40KB → 16KB (delta payloads are small)
- Broadcast interval: 3000ms → 1500ms (better UI responsiveness)
- BUG-025: timer keys restored, track_change=false handles it properly"
git push origin main
```

Оновити ACTION_PLAN.md — Sprint 1 повністю завершено.
