# Sprint 1 / Сесія 1a — Design Tokens + CSS Architecture

## Контекст

ModESP v4 — ESP32 контролер холодильного обладнання. WebUI на Svelte 4 (52KB gz).
Зараз UI має прототипний рівень дизайну: inconsistent spacing, typography, colors.
Це перша сесія з плану MVP→Production. Ми створюємо Design System foundation.

**Цільові користувачі:** холодильники-інженери (30-60 років), телефон/планшет, іноді рукавички.

## Задачі

### 1. Створити `webui/src/styles/tokens.css`

Єдине джерело правди для всіх design tokens. Файл імпортується глобально.

```css
/* Spacing scale (4px base) */
--spacing-1: 4px;
--spacing-2: 8px;
--spacing-3: 12px;
--spacing-4: 16px;
--spacing-6: 24px;
--spacing-8: 32px;
--spacing-12: 48px;

/* Typography scale */
--text-xs: 11px;    /* labels, hints */
--text-sm: 13px;    /* secondary text, badges */
--text-md: 14px;    /* body, descriptions */
--text-lg: 16px;    /* widget values, nav */
--text-xl: 18px;    /* page titles */
--text-2xl: 24px;   /* setpoint display */
--text-hero: 56px;  /* main temperature (mobile) */
--text-hero-desktop: 72px;

/* Font weights */
--font-normal: 400;
--font-medium: 500;
--font-semibold: 600;
--font-bold: 700;

/* Border radius */
--radius-sm: 6px;   /* inputs */
--radius-md: 8px;   /* buttons */
--radius-lg: 12px;  /* cards */
--radius-xl: 16px;  /* modals */

/* Semantic status colors (Industrial HMI standard) */
--status-compressor: #2563eb;  /* blue, NOT green */
--status-fan: #0891b2;         /* cyan */
--status-heater: #dc2626;      /* red */
--status-defrost: #f59e0b;     /* amber */
--status-idle: var(--fg-muted);
--status-ok: #16a34a;          /* green = normal/OK */

/* Alarm colors */
--alarm-bg: rgba(239, 68, 68, 0.08);
--alarm-border: #ef4444;
--alarm-text: #dc2626;

/* Touch targets */
--touch-min: 44px;  /* WCAG minimum */
--touch-comfortable: 48px;
```

### 2. Рефакторинг CSS vars в `webui/src/App.svelte`

Поточний стан: CSS custom properties визначені inline в App.svelte (:root).
Задача: витягнути всі визначення у tokens.css, залишити в App.svelte тільки import.

- Зберегти існуючі теми (light/dark) — вони мають різні `--bg`, `--card`, `--fg` etc.
- Додати нові semantic tokens поруч з існуючими
- НЕ ламати існуючі компоненти — вони продовжують працювати з старими vars
- Поступова міграція на нові tokens (не в цій сесії)

### 3. Audit існуючих hardcoded значень

Пройтися по основним компонентам та задокументувати де hardcoded значення замінити на tokens.
Це НЕ заміна зараз — тільки список для наступних сесій.

Компоненти для audit:
- `Card.svelte` — padding, font-size, border-radius
- `Layout.svelte` — spacing, nav sizing
- `Dashboard.svelte` — font sizes, colors, spacing
- `WidgetRenderer.svelte` — margins

## Файли

| Файл | Дія |
|------|-----|
| `webui/src/styles/tokens.css` | СТВОРИТИ |
| `webui/src/App.svelte` | РЕДАГУВАТИ (import tokens, не ламати існуючий CSS) |

## Критерії завершення

- [ ] `tokens.css` створено з повним набором tokens
- [ ] `App.svelte` імпортує tokens.css
- [ ] `npm run build` проходить без помилок
- [ ] Bundle size не збільшився більше ніж на 0.5KB gz
- [ ] Existing UI виглядає ідентично (ніяких visual regressions)
- [ ] Список hardcoded значень для міграції задокументований (коментар або окремий файл)

## Після завершення

```bash
cd webui && npm run build
# Verify bundle size
git add webui/src/styles/tokens.css webui/src/App.svelte
git commit -m "feat(webui): design tokens CSS architecture — spacing, typography, semantic colors"
git push origin main
```

Оновити ACTION_PLAN.md — відмітити сесію 1a як завершену.
