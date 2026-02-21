# Задача: WebUI — Select Widget + Disabled State

## Контекст

Features System реалізований. generate_ui.py генерує ui.json з:
- `"widget": "select"` + `"options": [{"value": 0, "label": "За часом"}, ...]`
- `"disabled": true` + `"disabled_reason": "Потрібно: evap_fan"`

WebUI (Svelte 4 у webui/) НЕ обробляє ці поля — їх потрібно додати.

### Поточний стан ui.json (реальні дані):

Select widgets:
- defrost.type — select, 1 option (constraints відфільтрували 2 з 3)
- defrost.counter_mode — select, 2 options
- defrost.initiation — select, 2 options + disabled
- thermostat.evap_fan_mode — select, 2 options + disabled

Disabled widgets (14 штук):
- thermostat.evap_fan_mode, fan_stop_temp, fan_stop_hyst, cond_fan_delay
- defrost.initiation, end_temp, demand_temp, fad_temp, stabilize_time, valve_delay, equalize_time
- protection.door_delay

### Файли WebUI:

```
webui/src/
  components/
    WidgetRenderer.svelte  — маршрутизація widget type → компонент
    Card.svelte            — контейнер для карточок
    widgets/
      NumberInput.svelte   — +/- кнопки + input (для числових параметрів)
      SliderWidget.svelte
      IndicatorWidget.svelte
      StatusText.svelte
      ToggleWidget.svelte
      ButtonWidget.svelte
      ValueWidget.svelte
      ... інші
  pages/
    DynamicPage.svelte     — рендерить сторінку з ui.json
```

## Що потрібно зробити

### 1. Створити SelectWidget.svelte

Новий файл: `webui/src/components/widgets/SelectWidget.svelte`

Dropdown (native `<select>`) з options зі ui.json.
Коли value змінюється → POST /api/settings.

```
config.options = [
  {"value": 0, "label": "За часом (зупинка компресора)"},
  {"value": 1, "label": "Електрична (тен)"}
]
config.key = "defrost.type"
config.description = "Тип відтайки (dFT)"
config.disabled = true/false
config.disabled_reason = "Потрібно: heater"
```

Вигляд:
- Label зверху (description)
- Native <select> з опціями
- Стиль як у NumberInput (однакові border, radius, color scheme)
- Якщо disabled — сірий, не клікабельний, під select текст disabled_reason

### 2. Додати select в WidgetRenderer.svelte

```js
import SelectWidget from './widgets/SelectWidget.svelte';

const widgetMap = {
  // існуючі...
  select: SelectWidget,
};
```

### 3. Disabled state для ВСІХ editable widgets

Кожен editable widget (NumberInput, SliderWidget, SelectWidget, ToggleWidget)
повинен обробляти `config.disabled`:
- Візуально: opacity 0.5, pointer-events: none
- Показувати config.disabled_reason дрібним текстом під widget
- Read-only widgets (ValueWidget, IndicatorWidget, StatusText) — НЕ потребують disabled

Найпростіший спосіб — обернути в WidgetRenderer.svelte:

```svelte
<div class="widget-wrapper" class:disabled={widget.disabled}>
  <svelte:component this={component} config={widget} {value} />
  {#if widget.disabled && widget.disabled_reason}
    <div class="disabled-reason">{widget.disabled_reason}</div>
  {/if}
</div>
```

### 4. Build та deploy

```bash
cd webui
npm run build
npm run deploy   # → data/www/
```

## Стиль

Дотримуватись існуючого стилю WebUI:
- CSS variables: var(--bg), var(--card), var(--border), var(--fg), var(--fg-muted), var(--accent)
- border-radius: 6-8px
- font-size: 14px для labels, 16px для values
- Анімації: transition 0.15s

disabled-reason:
- font-size: 11px
- color: var(--fg-muted) з opacity
- margin-top: 4px
- Іконка 🔒 або без іконки

## Чого НЕ робити

- НЕ змінювати generate_ui.py (дані вже правильні)
- НЕ змінювати маніфести
- НЕ змінювати C++ код
- НЕ додавати нові npm залежності
- НЕ міняти існуючі widgets які працюють правильно

## Перевірка

```bash
cd webui && npm run build && npm run deploy

# Перевірити що SelectWidget рендериться для select widgets
# Перевірити що disabled widgets сірі з reason текстом
# Перевірити що defrost.type dropdown показує тільки доступні типи
```
