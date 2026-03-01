# Sprint 3 / Сесія 1.4 — Mobile UX Polish

## Контекст

Sprint 1-2 завершено: Design System, delta broadcasts, Dashboard redesign, settings debounce.
Мобільний — основний use case (інженери з телефоном в холодній кімнаті).

## Проблеми зараз

1. Bottom tabs: 9+ сторінок → горизонтальний scroll, labels 10px нечитабельні
2. Tablet (768-1024px): 2 вузькі колонки — крамп
3. NumberInput кнопки 36px — менше WCAG мінімуму 44px
4. Немає env(safe-area-inset-bottom) для notched phones

## Задачі

### 1. Bottom Tabs — Max 5 + "Ще..."

Файл: `webui/src/components/Layout.svelte`

- Показувати максимум 5 tabs: Dashboard, Thermostat, Defrost, Protection, **Ще...**
- "Ще..." (More) відкриває fullscreen overlay з повним списком сторінок
- Tab labels: 10px → `--text-xs` (11px)
- Active tab: `--accent` підкреслення (2px bottom border)
- Tab icon + label vertical layout, min-height 52px
- `env(safe-area-inset-bottom)` padding для notched phones (iPhone X+)

### 2. Tablet Layout — Single Column

Файл: `webui/src/pages/DynamicPage.svelte`

- Breakpoint 768-1024px: залишити **1 колонку** (зараз 2 — замалі)
- Max-width: 640px, centered
- 2 колонки тільки при >1024px
- Cards: full width в single column mode

### 3. Touch Targets — 44px Minimum

Файли: всі interactive widgets

- NumberInput buttons (+/-): 36px → **44px** min-height
- Button widget: перевірити min-height
- Select widget: dropdown arrow area → 44px
- Toggle: switch area → 44px
- Checkbox: area → 44px

### 4. Sidebar Tablet View

Файл: `webui/src/components/Layout.svelte`

- 768-1024px: sidebar collapsed 64px → додати **tooltip** на icon hover
- Або: прибрати sidebar на tablet, залишити bottom tabs як на mobile

## Файли

| Файл | Дія |
|------|-----|
| `webui/src/components/Layout.svelte` | РЕДАГУВАТИ (tabs, sidebar) |
| `webui/src/pages/DynamicPage.svelte` | РЕДАГУВАТИ (grid breakpoints) |
| `webui/src/components/widgets/NumberInput.svelte` | РЕДАГУВАТИ (button size) |

## Критерії завершення

- [ ] Bottom tabs: max 5 + "Ще" overlay, no scroll
- [ ] Tab labels readable (11px+)
- [ ] Tablet: single column, centered
- [ ] Всі interactive елементи ≥44px
- [ ] iPhone X+ notch: bottom tabs не перекриваються
- [ ] `npm run build` < 55KB gz
- [ ] Test: Chrome DevTools → responsive mode → iPhone SE, iPad, iPad Pro

## Після завершення

```bash
cd webui && npm run build && npm run deploy
git add webui/src/components/Layout.svelte webui/src/pages/DynamicPage.svelte \
  webui/src/components/widgets/NumberInput.svelte
git commit -m "feat(webui): mobile UX — 5+More tabs, 44px touch targets, tablet single column"
git push origin main
```
