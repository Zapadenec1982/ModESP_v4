# Sprint 2 / Сесія 1.2b — Slider + MiniChart Improvements

## Контекст

Dashboard layout переосмислено (сесія 1.2a). Тепер покращуємо ключові interactive елементи.

## Задачі

### 1. SliderWidget — Touch-friendly

Файл: `webui/src/components/widgets/SliderWidget.svelte`

- Track height: 6px → **10px** (рядок ~55)
- Thumb size: 22px → **36px** (рядок ~61), hit area 48px через padding
- Додати **min/max labels** на кінцях slider track (text `--text-xs`)
- Додати **step indicator** під slider ("крок: 0.5°C")
- Hover state: cursor pointer + slight track highlight
- **Pending save indicator:**
  - on:input → оновити display value + показати subtle "unsaved" dot
  - Debounce 300ms
  - on:change або після debounce → POST
  - При success → brief green flash (200ms)
  - При error → revert до серверного значення + red toast

### 2. MiniChart — Interactive

Файл: `webui/src/components/MiniChart.svelte`

- **Touch/Click tooltip:** при tap/hover показати час та значення
- **"X хвилин тому"** текст під графіком (відносний час останнього sample)
- **"Детальний графік →"** посилання під chart → навігація на DataLogger page
- Збільшити висоту з поточної на 100px (mobile) / 120px (desktop)
- Показати setpoint horizontal line (якщо є в $state)

### 3. Dashboard Setpoint Slider Integration

Файл: `webui/src/pages/Dashboard.svelte`

- Inline slider під hero temperature (не окремий блок)
- Показувати поточне значення + одиниці ("SP: -20.0°C")
- Slider використовує SliderWidget з оновленим UX
- Min/max з state_meta

## Файли

| Файл | Дія |
|------|-----|
| `webui/src/components/widgets/SliderWidget.svelte` | РЕДАГУВАТИ |
| `webui/src/components/MiniChart.svelte` | РЕДАГУВАТИ |
| `webui/src/pages/Dashboard.svelte` | РЕДАГУВАТИ (slider integration) |

## Критерії завершення

- [ ] Slider: thumb 36px+, track 10px, min/max labels видимі
- [ ] Slider: debounce + pending/success/error feedback
- [ ] MiniChart: tooltip при touch, relative time, link to DataLogger
- [ ] Mobile: slider легко використовувати пальцем
- [ ] `npm run build` < 55KB gz

## Після завершення

```bash
cd webui && npm run build && npm run deploy
git add webui/src/components/widgets/SliderWidget.svelte \
  webui/src/components/MiniChart.svelte webui/src/pages/Dashboard.svelte
git commit -m "feat(webui): slider touch-friendly (36px thumb, debounce) + MiniChart interactive"
git push origin main
```
