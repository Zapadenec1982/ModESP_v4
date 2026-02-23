# CLAUDE_TASK: Settings Validation — visible_when + Disabled Options

> Системна валідація "обладнання → налаштування → UI".
> Користувач бачить тільки актуальне для його конфігурації.
> Єдиний механізм для всіх модулів — існуючих і майбутніх.

---

## Статус задач

| # | Задача | Статус | Деталі |
|---|--------|--------|--------|
| 1 | Маніфести: constraints + visible_when | TODO | defrost, thermostat, protection |
| 2 | generate_ui.py: requires_state на options | TODO | resolve_constraints() → requires_state + hint |
| 3 | generate_ui.py: visible_when passthrough | TODO | _build_widget() + _module_page() |
| 4 | SelectWidget.svelte: runtime disabled options | TODO | $state[requires_state] перевірка |
| 5 | Svelte: visible_when (DynamicPage) | TODO | isVisible() + $state reactivity |
| 6 | Equipment: додати has_* ключі | TODO | has_cond_fan, has_door_contact, has_evap_temp |
| 7 | Тести + build | TODO | pytest, npm run build, idf.py build |

---

## Контекст проблеми

### Баг-тригер (BUG-011)
В конфігурації не призначений клапан гарячого газу (hg_valve).
В налаштуваннях обрано відтайку гарячим газом (defrost.type = 2).
Коли почалась відтайка — компресор працював, але клапан відсутній →
холодильник продовжив охолодження замість відтайки.

### Системна проблема
Це не баг одного модуля — це архітектурна проблема фреймворку.
**Будь-який модуль, що залежить від опціонального обладнання, може мовчки працювати неправильно.**

UI показує ВСІ налаштування незалежно від конфігурації обладнання.
Це вводить користувача в оману і виглядає як захаращена кімната.

Приклади в існуючих модулях:
- **Defrost**: `defrost.type = 2` (hotgas) без hg_valve
- **Defrost**: `defrost.type = 1` (heater) без heater
- **Defrost**: `initiation = 1` (demand по T) без evap_temp
- **Thermostat**: `evap_fan_mode = 2` (за T випарника) без evap_temp
- **Thermostat**: `night_mode = 2` (через DI) без night_input
- **Thermostat**: `cond_fan_delay` видимий, але cond_fan не сконфігурований
- **Protection**: `door_delay` видимий, але door_contact не сконфігурований

Кожен новий модуль матиме ті ж ризики. Рішення має бути на рівні маніфестів.

---

## Архітектурне рішення

### Два незалежних механізми — обидва runtime в Svelte

**1. Disabled options** — runtime (Svelte, перевірка `equipment.has_*`)
- generate_ui.py кладе ВСІ options в ui.json + поле `requires_state` і `disabled_hint`
- SelectWidget перевіряє `$state[opt.requires_state]` в реальному часі
- Немає hg_valve → option "Гарячий газ" сіра. Призначив реле → option одразу доступна
- Без перекомпіляції, без перезавантаження сторінки

**2. visible_when** — runtime (Svelte, перевірка значення state key)
- Віджети/картки показуються тільки коли залежний state key має відповідне значення
- Користувач вибирає `defrost.type = 2` → з'являється картка "Гарячий газ"
- Вибирає `defrost.type = 0` → картка зникає

### Ланцюг захисту

```
disabled option → не можна обрати → state key не отримає це значення
→ visible_when не спрацює → залежна картка прихована
```

Приклад: немає hg_valve → option "Гарячий газ" сіра → defrost.type ≠ 2 → картка "Гарячий газ" прихована.
Є hg_valve → option активна → обрав → defrost.type = 2 → картка з'являється.

### Що НЕ потрібно

| Ідея | Чому ні |
|------|---------|
| features в SharedState | `equipment.has_heater`, `equipment.has_hg_valve` **вже опубліковані** в SharedState |
| constraints_meta.h (новий артефакт) | Backend може валідувати по options list з state_meta, або runtime fallback ловить |
| Складна NVS міграція при зміні bindings | При прошивці `idf.py erase-flash` → PersistService пише defaults з маніфестів |
| visible_when по feature compile-time | Ланцюг "disabled option → hidden card" працює без окремого механізму |

---

## Що вже є (інвентаризація)

### Backend — generate_ui.py

| Компонент | Файл : рядок | Що робить | Що змінити |
|-----------|-------------|-----------|------------|
| `resolve_constraints()` | generate_ui.py:591 | **Видаляє** options де feature неактивна | Замінити видалення на `disabled: true` + `disabled_hint` |
| `_build_widget()` | generate_ui.py:743 | Будує widget dict, мерджить state_meta | Додати passthrough `visible_when` |
| `_module_page()` | generate_ui.py:720 | Будує page з cards | Додати passthrough `visible_when` на cards |
| `get_disabled_info()` | generate_ui.py:618 | disabled + reason для цілого widget | Без змін (ортогональний механізм) |
| `FeatureResolver` | generate_ui.py | Обчислює active features з bindings | Без змін |

### Backend — C++

| Компонент | Файл | Стан |
|-----------|------|------|
| `equipment.has_heater` | equipment_module.cpp:96 | Опубліковано в SharedState ✅ |
| `equipment.has_hg_valve` | equipment_module.cpp:97 | Опубліковано в SharedState ✅ |
| `has_feature()` | BaseModule | constexpr lookup по features_config.h ✅ |
| Defrost runtime fallback | defrost_module.cpp | hotgas→natural, heater→natural ✅ |
| POST /api/settings | http_service.cpp:380 | min/max clamp, **НЕМАє** enum валідації |

### Frontend — Svelte

| Компонент | Файл | Стан |
|-----------|------|------|
| `SelectWidget` | SelectWidget.svelte | Рендерить `config.options` (вже відфільтровані). Є disabled для цілого widget, **НЕМАЄ** disabled per-option |
| `DynamicPage` | DynamicPage.svelte:14 | `{#each page.cards as card}` — **НЕМАЄ** visible_when |
| `Card` | Card.svelte | Має collapsible, **НЕМАЄ** visible_when |
| `WidgetRenderer` | WidgetRenderer.svelte | Просто рендерить widget, **НЕМАЄ** visible_when |
| `state` store | stores/state.js | Reactive store, оновлюється через WS |
| `setStateKey()` | stores/state.js:24 | Оптимістичний update (локально до відповіді сервера) |

### Маніфести — constraints

| Модуль | Constraints | Стан |
|--------|------------|------|
| Defrost | `"constraints": {}` | **ПОРОЖНІ** — всі 3 options defrost.type показані |
| Thermostat | evap_fan_mode, night_mode | 2 constraints визначені ✅ |
| Protection | Немає constraints | Немає select-залежних settings |

### features_config.h (поточний стан, 13 features)

```
defrost_timer=true, defrost_by_sensor=false, defrost_electric=false, defrost_hot_gas=false
basic_protection=true, door_protection=true
basic_cooling=true, fan_control=true, fan_temp_control=false, condenser_fan=true
night_setback=true, night_di=false, display_defrost=true
```

---

## Task 1: Маніфести — constraints + visible_when

### 1.1 Defrost: додати constraints (зараз порожні!)

**Файл:** `modules/defrost/manifest.json`

```json
"constraints": {
  "defrost.type": {
    "type": "enum_filter",
    "values": {
      "0": {"always": true},
      "1": {"requires_feature": "defrost_electric", "disabled_hint": "Потрібен тен (heater)"},
      "2": {"requires_feature": "defrost_hot_gas", "disabled_hint": "Потрібен клапан ГГ (hg_valve)"}
    }
  },
  "defrost.initiation": {
    "type": "enum_filter",
    "values": {
      "0": {"always": true},
      "1": {"requires_feature": "defrost_by_sensor", "disabled_hint": "Потрібен датчик випарника (evap_temp)"},
      "2": {"requires_feature": "defrost_by_sensor", "disabled_hint": "Потрібен датчик випарника (evap_temp)"},
      "3": {"always": true}
    }
  }
}
```

### 1.2 Defrost: visible_when на віджетах

В секції `ui.cards` маніфесту, додати `visible_when` до widget definitions:

**Картка "Гарячий газ"** (stabilize_time, valve_delay, equalize_time):
```json
{
  "title": "Гарячий газ",
  "visible_when": {"key": "defrost.type", "eq": 2},
  "widgets": [
    {"key": "defrost.stabilize_time", "widget": "number_input"},
    {"key": "defrost.valve_delay", "widget": "number_input"},
    {"key": "defrost.equalize_time", "widget": "number_input"}
  ]
}
```

**Окремі віджети:**
```json
{"key": "defrost.demand_temp", "widget": "number_input",
 "visible_when": {"key": "defrost.initiation", "in": [1, 2]}},

{"key": "defrost.end_temp", "widget": "number_input",
 "visible_when": {"key": "defrost.initiation", "in": [1, 2]}},

{"key": "defrost.fad_temp", "widget": "number_input",
 "visible_when": {"key": "defrost.initiation", "in": [1, 2]}}
```

### 1.3 Thermostat: visible_when на віджетах

```json
{"key": "thermostat.fan_stop_temp", "widget": "number_input",
 "visible_when": {"key": "thermostat.evap_fan_mode", "eq": 2}},

{"key": "thermostat.fan_stop_hyst", "widget": "number_input",
 "visible_when": {"key": "thermostat.evap_fan_mode", "eq": 2}},

{"key": "thermostat.cond_fan_delay", "widget": "number_input",
 "visible_when": {"key": "equipment.has_cond_fan", "eq": true}},

{"key": "thermostat.night_start", "widget": "number_input",
 "visible_when": {"key": "thermostat.night_mode", "eq": 1}},

{"key": "thermostat.night_end", "widget": "number_input",
 "visible_when": {"key": "thermostat.night_mode", "eq": 1}},

{"key": "thermostat.night_setback", "widget": "number_input",
 "visible_when": {"key": "thermostat.night_mode", "in": [1, 2, 3]}}
```

### 1.4 Protection: visible_when на door_delay

```json
{"key": "protection.door_delay", "widget": "number_input",
 "visible_when": {"key": "equipment.has_door_contact", "eq": true}}
```

> **Примітка:** equipment.has_door_contact потрібно додати в SharedState (equipment_module.cpp).
> Наразі є лише equipment.has_heater і equipment.has_hg_valve.

### Формат visible_when

```json
{"key": "defrost.type", "eq": 2}           // == 2
{"key": "thermostat.night_mode", "neq": 0}  // != 0
{"key": "thermostat.night_mode", "in": [1, 2, 3]}  // в списку
```

Тільки 3 оператори: `eq`, `neq`, `in`. Без вкладених AND/OR — не потрібні на цьому етапі.

---

## Task 2: generate_ui.py — requires_state на options

**Файл:** `tools/generate_ui.py`, рядок 591

### Поточна поведінка (ПРОБЛЕМА):

`resolve_constraints()` (рядок 605-613) **видаляє** options де feature неактивна.
Результат запечений в ui.json при build. Зміна bindings не впливає без rebuild.

### Нова поведінка (РІШЕННЯ):

`resolve_constraints()` **завжди додає ВСІ options** + поле `requires_state` і `disabled_hint`
для тих що залежать від обладнання. Svelte перевіряє `$state[requires_state]` в runtime.

**Маппінг feature → state key** (в resolve_constraints):

| Feature | requires_state |
|---------|---------------|
| `defrost_electric` | `equipment.has_heater` |
| `defrost_hot_gas` | `equipment.has_hg_valve` |
| `defrost_by_sensor` | `equipment.has_evap_temp` |
| `fan_temp_control` | `equipment.has_evap_temp` |
| `night_di` | `equipment.has_night_input` |

```python
# Маппінг feature → equipment.has_* state key
FEATURE_TO_STATE = {
    "defrost_electric":  "equipment.has_heater",
    "defrost_hot_gas":   "equipment.has_hg_valve",
    "defrost_by_sensor": "equipment.has_evap_temp",
    "fan_temp_control":  "equipment.has_evap_temp",
    "night_di":          "equipment.has_night_input",
}

filtered = []
for opt in original_options:
    val_str = str(opt["value"])
    rule = constraint.get("values", {}).get(val_str, {})
    new_opt = dict(opt)
    feat = rule.get("requires_feature")
    if feat and feat in FEATURE_TO_STATE:
        new_opt["requires_state"] = FEATURE_TO_STATE[feat]
        new_opt["disabled_hint"] = rule.get("disabled_hint", "Недоступно")
    filtered.append(new_opt)  # ЗАВЖДИ додаємо
```

**Результат в ui.json:**
```json
"options": [
  {"value": 0, "label": "За часом (зупинка)"},
  {"value": 1, "label": "Електрична (тен)",
   "requires_state": "equipment.has_heater",
   "disabled_hint": "Потрібен тен (heater)"},
  {"value": 2, "label": "Гарячий газ",
   "requires_state": "equipment.has_hg_valve",
   "disabled_hint": "Потрібен клапан ГГ (hg_valve)"}
]
```

> **Ключова різниця:** `disabled: true` НЕ запечений. Svelte сам вирішує
> на основі `$state["equipment.has_hg_valve"]`. Призначив реле → опція одразу доступна.

---

## Task 3: generate_ui.py — visible_when passthrough

### 3a. В _build_widget() (рядок 743)

Після створення widget dict, прокинути visible_when:
```python
if "visible_when" in w:
    widget["visible_when"] = w["visible_when"]
```

### 3b. В _module_page() (рядок 720)

При побудові card dict, прокинути visible_when:
```python
for card in ui.get("cards", []):
    page_card = {"title": card["title"], "widgets": []}
    if "visible_when" in card:
        page_card["visible_when"] = card["visible_when"]
    # ... existing logic ...
```

### 3c. Валідація visible_when в ManifestValidator

Додати V19 — visible_when format validation:
- `key` повинен існувати в global state map
- `eq`/`neq` — скаляр, `in` — масив
- Не більше одного оператора

---

## Task 4: SelectWidget.svelte — runtime disabled options

**Файл:** `webui/src/components/widgets/SelectWidget.svelte`

### Поточний код (рядок 23-27):
```svelte
<select class="select-input" value={current} on:change={onChange} {disabled}>
  {#each options as opt}
    <option value={opt.value}>{opt.label}</option>
  {/each}
</select>
```

### Новий код:
```svelte
<script>
  import { state } from '../../stores/state.js';

  // Runtime: перевірка requires_state через $state
  $: enriched = options.map(opt => ({
    ...opt,
    isDisabled: opt.requires_state ? !$state[opt.requires_state] : false
  }));

  // Hint для поточного disabled значення (якщо NVS має старе значення)
  $: currentOpt = enriched.find(o => o.value === current);
  $: hint = currentOpt?.isDisabled ? currentOpt.disabled_hint : null;
</script>

<select class="select-input" value={current} on:change={onChange} {disabled}>
  {#each enriched as opt}
    <option value={opt.value} disabled={opt.isDisabled}>
      {opt.label}
    </option>
  {/each}
</select>
{#if hint}
  <div class="disabled-hint">{hint}</div>
{/if}
```

> **Реактивність:** `$state` оновлюється через WebSocket. Призначив реле для hg_valve →
> `equipment.has_hg_valve` стає true → `enriched` перераховується → option розблоковується.
> Без перезавантаження сторінки, без rebuild.

**CSS:**
```css
.disabled-hint {
  font-size: 11px;
  color: var(--warning, #f39c12);
  margin-top: 4px;
}
```

---

## Task 5: Svelte — visible_when + localSettings

### 5a. Проблема реактивності

visible_when залежить від ПОТОЧНОГО значення state key.
Коли користувач змінює select — setStateKey() оновлює `$state` оптимістично.
Це вже працює для visible_when, бо `$state` реактивний.

**АЛЕ**: якщо API POST повертає помилку, $state вже змінений локально.
Наступний WS broadcast перезапише значення на серверне.
Це ОК для visible_when — UI повернеться до стану сервера.

> **Рішення:** Використовуємо існуючий `$state` store + `setStateKey()`.
> Окремий localSettings store НЕ ПОТРІБЕН — setStateKey вже забезпечує
> оптимістичний update, а WS broadcast виправляє при помилці.

### 5b. Функція isVisible()

Створити `webui/src/lib/visibility.js`:

```javascript
/**
 * Перевірити visible_when умову.
 * @param {object} vw  — visible_when з ui.json
 * @param {object} st  — поточний $state
 * @returns {boolean}
 */
export function isVisible(vw, st) {
  if (!vw) return true;
  if ('eq' in vw)  return st[vw.key] === vw.eq;
  if ('neq' in vw) return st[vw.key] !== vw.neq;
  if ('in' in vw)  return vw.in.includes(st[vw.key]);
  return true;
}
```

### 5c. DynamicPage.svelte — visible_when на cards + widgets

**Файл:** `webui/src/pages/DynamicPage.svelte`

Поточний код (рядок 12-20):
```svelte
{#each page.cards as card}
  <Card title={card.title} collapsible={card.collapsible || false}>
    {#each card.widgets as widget}
      <WidgetRenderer {widget} value={$state[widget.key]} />
    {/each}
  </Card>
{/each}
```

Новий код:
```svelte
<script>
  import { isVisible } from '../lib/visibility.js';
</script>

{#each page.cards as card}
  {#if isVisible(card.visible_when, $state)}
    <Card title={card.title} collapsible={card.collapsible || false}>
      {#each card.widgets as widget}
        {#if isVisible(widget.visible_when, $state)}
          <WidgetRenderer {widget} value={$state[widget.key]} />
        {/if}
      {/each}
    </Card>
  {/if}
{/each}
```

> **Реактивність:** `$state` — Svelte reactive store. Коли setStateKey() змінює
> значення (рядок 24 state.js), Svelte автоматично перевіряє всі {#if} блоки.
> Картки з'являються/зникають миттєво при зміні select.

---

## Task 6: Equipment — додати has_* ключі

**Файл:** `modules/equipment/src/equipment_module.cpp`

В `bind_drivers()` або `on_init()`:
```cpp
state_set("equipment.has_heater", heater_ != nullptr);       // ✅ вже є
state_set("equipment.has_hg_valve", hg_valve_ != nullptr);    // ✅ вже є
state_set("equipment.has_cond_fan", cond_fan_ != nullptr);    // ДОДАТИ
state_set("equipment.has_door_contact", door_sensor_ != nullptr);  // ДОДАТИ
state_set("equipment.has_evap_temp", sensor_evap_ != nullptr);     // ДОДАТИ (для visible_when)
```

**Файл:** `modules/equipment/manifest.json`

Додати state keys:
```json
"equipment.has_cond_fan": {"type": "bool", "access": "read", "description": "Вент. конденсатора доступний"},
"equipment.has_door_contact": {"type": "bool", "access": "read", "description": "Контакт дверей доступний"},
"equipment.has_evap_temp": {"type": "bool", "access": "read", "description": "Датчик випарника доступний"}
```

---

## Task 7: Тести + Build

### pytest
- Тест: resolve_constraints() повертає disabled options замість фільтрованих
- Тест: visible_when прокидується в ui.json (widget + card level)
- Тест: V19 валідація visible_when формату
- Тест: defrost.type constraints з disabled_hint
- Оновити кількість state_meta ключів (+3 equipment.has_*)

### WebUI
```bash
cd webui && npm run build && npm run deploy
```

### ESP32
```bash
idf.py build
```

---

## Порядок реалізації

```
Task 6 (equipment has_*) → можна паралельно з Task 1-3
Task 1 (маніфести) → Task 2 (requires_state) → Task 3 (visible_when passthrough)
→ pytest + generate_ui.py
Task 4 (SelectWidget) → Task 5 (visible_when Svelte) → npm run build
Task 6 (equipment) → idf.py build
Task 7 (фінальний тест)
```

**Оцінка:** ~4-5 годин роботи, ~15 файлів, ~200 нових рядків.

---

## NVS та значення за замовчуванням

При прошивці з `idf.py erase-flash` — NVS очищається → PersistService при boot
записує defaults з `state_meta.h` (згенеровані з маніфестів).

Це знімає проблему "старі значення в NVS після зміни bindings".
Складна міграція/валідація NVS НЕ ПОТРІБНА.

Сценарії:
1. **Прошивка з erase** → NVS чистий → defaults → все валідно
2. **Зміна bindings через WebUI** → disabled options в UI не дадуть обрати невалідне

> **На майбутнє:** додати "Скинути до заводських" в меню WebUI (erase NVS + restart).

---

## Існуючий код — reference

### generate_ui.py
- `resolve_constraints()` → рядок 591 (фільтрація options)
- `_build_widget()` → рядок 743 (побудова widget dict)
- `_module_page()` → рядок 720 (побудова page з cards)
- `get_disabled_info()` → рядок 618 (disabled для цілого widget)
- `FeatureResolver` → обчислює active features з bindings

### Svelte
- `SelectWidget.svelte` → 51 рядок, config.options, native disabled
- `DynamicPage.svelte` → 47 рядків, {#each page.cards}, {#each card.widgets}
- `WidgetRenderer.svelte` → 62 рядки, svelte:component dispatch
- `Card.svelte` → 83 рядки, title + collapsible + slot
- `state.js` → writable store, setStateKey() оптимістичний update

### C++
- `equipment_module.cpp:96-97` → has_heater, has_hg_valve в SharedState (додати has_cond_fan, has_door_contact, has_evap_temp)

---

## Ризики та нюанси

1. **setStateKey + visible_when timing**: setStateKey() оптимістично оновлює $state.
   Якщо POST повертає помилку — наступний WS broadcast поверне серверне значення.
   Картка може "блимнути" (з'явитися і зникнути). Маловірогідно на практиці.

2. **SharedState capacity**: Зараз 81 ключ, ліміт 128. Додаємо +3 (has_cond_fan,
   has_door_contact, has_evap_temp) = 84. Запас достатній.

3. **options disabled + поточне значення**: Якщо в NVS збережено defrost.type=2,
   але hg_valve прибрали → select покаже "Гарячий газ" (поточне) як disabled.
   Після erase-flash + прошивка → default (0). Або factory reset через меню (майбутнє).

4. **visible_when в існуючих маніфестах**: Додається тільки в ui.cards секцію.
   Якщо маніфест не має visible_when — поведінка не змінюється (isVisible повертає true).

---

## Критерій завершення

- [ ] Опція "Гарячий газ" в select — сіра з підказкою якщо hg_valve відсутній
- [ ] Опція "Електрична (тен)" — сіра з підказкою якщо heater відсутній
- [ ] Картка "Гарячий газ" — прихована якщо defrost.type ≠ 2
- [ ] Thermostat night_start/end — приховані якщо night_mode ≠ 1
- [ ] Thermostat fan_stop_temp/hyst — приховані якщо evap_fan_mode ≠ 2
- [ ] Protection door_delay — прихований якщо equipment.has_door_contact = false
- [ ] equipment.has_cond_fan, has_door_contact, has_evap_temp в SharedState
- [ ] Призначив реле для hg_valve → опція "Гарячий газ" одразу розблоковується (без rebuild)
- [ ] Аналогічно працює для defrost.initiation (demand/combo disabled без evap_temp)
- [ ] Новий модуль отримує валідацію через маніфест (constraints + visible_when)
- [ ] 206+ pytest тестів зелені
- [ ] WebUI build без помилок
- [ ] idf.py build без помилок

---

## Changelog
- 2026-02-23 — Переписано після обговорення. Спрощена архітектура: 2 механізми
  (disabled options compile-time + visible_when runtime). Прибрано features в SharedState,
  constraints_meta.h, складну NVS міграцію. Додано localSettings аналіз (не потрібен —
  setStateKey вже є). Додано equipment.has_* ключі. Backend enum валідація → optional.
- 2026-02-22 — Створено. BUG-011 як тригер системної валідації.
