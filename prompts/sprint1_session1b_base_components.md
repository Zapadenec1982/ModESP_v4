# Sprint 1 / Сесія 1b — Base Components Refactor

## Контекст

Попередня сесія (1a) створила Design System tokens (`webui/src/styles/tokens.css`).
Тепер рефакторимо базові компоненти щоб використовувати нові tokens та покращити UX.

## Задачі

### 1. Card.svelte — Variants

Файл: `webui/src/components/Card.svelte`

- Додати prop `variant`: "default" | "status" | "alarm"
- `variant="alarm"` — червоний бордер (`--alarm-border`) + subtle bg (`--alarm-bg`)
- `variant="status"` — для dashboard status cards (slightly different bg)
- Використовувати `--radius-lg` (12px) замість hardcoded border-radius
- Padding: `--spacing-4` (16px) consistently для title та body
- Title font: `--text-xs` uppercase з letter-spacing 1px (замість hardcoded 11px)
- Collapsible: зберігати стан у sessionStorage (зараз втрачається при reload)

### 2. Toast.svelte — Bottom Center + Auto-dismiss

Файл: `webui/src/components/Toast.svelte`

- Перемістити з top-right у **bottom-center** (mobile-friendly, завжди в полі зору)
- Auto-dismiss timing:
  - success: 3 секунди
  - warning: 5 секунд
  - error: 8 секунд (зараз error = 5с, замало для читання)
- Додати кнопку закриття (X) — зараз dismiss тільки по click на весь toast
- Max 3 toasts одночасно — старіші зникають
- Z-index: перевірити що вище bottom tabs на mobile
- Анімація: slide up from bottom (замість fade in from top-right)

### 3. Layout.svelte — Connection Overlay

Файл: `webui/src/components/Layout.svelte`

- Коли `wsConnected` = false протягом **5+ секунд** (не одразу — уникнути flicker):
  - Показати centered overlay: "З'єднання втрачено. Перепідключення..."
  - Spinner animation
  - Кнопка "Перепідключити" (manual retry)
  - Overlay напів-прозорий — НЕ блокує сторінку (користувач бачить stale дані)
- Коли reconnected: зелений toast "З'єднано", GET /api/state для оновлення

### 4. WidgetRenderer.svelte — Consistent Spacing

Файл: `webui/src/components/WidgetRenderer.svelte`

- Gap між widgets: `--spacing-3` (12px) — уніфікувати
- Додати optional thin separator line між widgets (1px `--border-color`)
- Ensure кожен widget має min-height `--touch-min` (44px) якщо interactive

## Файли

| Файл | Дія |
|------|-----|
| `webui/src/components/Card.svelte` | РЕДАГУВАТИ |
| `webui/src/components/Toast.svelte` | РЕДАГУВАТИ |
| `webui/src/components/Layout.svelte` | РЕДАГУВАТИ |
| `webui/src/components/WidgetRenderer.svelte` | РЕДАГУВАТИ |

## Критерії завершення

- [ ] Card variants працюють: default/status/alarm
- [ ] Toast: bottom-center, auto-dismiss працює (3/5/8с), close button, max 3
- [ ] Connection overlay з'являється через 5с disconnect, зникає при reconnect
- [ ] Widget spacing consistent (12px gap)
- [ ] `npm run build` проходить
- [ ] Bundle size < 55KB gz
- [ ] Manual test: відключити WS (стоп сервер) → overlay через 5с → підключити → overlay зникає

## Після завершення

```bash
cd webui && npm run build && npm run deploy
git add webui/src/components/Card.svelte webui/src/components/Toast.svelte \
  webui/src/components/Layout.svelte webui/src/components/WidgetRenderer.svelte
git commit -m "feat(webui): base components refactor — Card variants, Toast bottom-center, connection overlay"
git push origin main
```

Оновити ACTION_PLAN.md — відмітити сесію 1b як завершену.
