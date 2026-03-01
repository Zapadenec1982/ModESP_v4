# Sprint 2 / Сесія 1.3a — Debounce + Form Behavior

## Контекст

Dashboard та slider оновлені. Тепер фіксимо головну UX проблему settings pages:
кожен клік +/- в NumberInput одразу відправляє POST. Немає feedback чи зміна збереглась.

## Задачі

### 1. Створити shared utility `webui/src/lib/settings.js`

Централізований sender для всіх widget POST operations:

```javascript
/**
 * createSettingSender(key, options?)
 * Returns: { send, pending, error, cleanup }
 *
 * Features:
 * - Debounce (default 500ms)
 * - Optimistic local state update (setStateKey)
 * - Error rollback to server value
 * - Toast on success/error
 * - Loading indicator via pending store
 */
export function createSettingSender(key, { debounceMs = 500 } = {}) {
  let timer;
  const pending = writable(false);
  const error = writable(null);

  async function send(value) {
    clearTimeout(timer);
    // Optimistic update
    setStateKey(key, value);
    pending.set(true);
    error.set(null);

    timer = setTimeout(async () => {
      try {
        await apiPost('/api/settings', { [key]: value });
        pending.set(false);
        // Brief success flash (handled by component)
      } catch (e) {
        pending.set(false);
        error.set(e.message);
        // Rollback: re-fetch from server
        toastError(e.message);
      }
    }, debounceMs);
  }

  function cleanup() { clearTimeout(timer); }

  return { send, pending, error, cleanup };
}
```

### 2. NumberInput — Debounce

Файл: `webui/src/components/widgets/NumberInput.svelte`

Зараз (рядок ~29): кожен клік +/- → `send()` → `apiPost()`. Rapid clicking = flood.

Fix:
- Використати `createSettingSender(key)` замість прямого apiPost
- +/- кнопки: оновити display одразу, debounce POST 500ms
- Показати "unsaved" indicator (colored dot) коли local ≠ server
- Success: brief green border flash
- Кнопки: min-height `--touch-min` (44px), зараз 36px

### 3. ToggleWidget — Loading State

Файл: `webui/src/components/widgets/ToggleWidget.svelte`

Зараз (рядок ~16): toggle → apiPost одразу. Немає feedback.

Fix:
- Використати `createSettingSender(key, { debounceMs: 0 })` (instant для toggle)
- Pending state: toggle dimmed + spinner
- Error: revert toggle position + red toast

### 4. SelectWidget — Loading State

Файл: `webui/src/components/widgets/SelectWidget.svelte`

Зараз (рядок ~28): change → apiPost одразу. Немає feedback.

Fix:
- Використати `createSettingSender(key, { debounceMs: 0 })`
- Pending: select dimmed + loading text
- Error: revert to previous value + red toast

### 5. Visual Feedback Pattern

Для всіх widgets — єдиний pattern:
- **Pending:** subtle opacity 0.7 + tiny spinner/dot
- **Success:** brief green border flash (200ms) → normal
- **Error:** red border + toast → revert to server value
- Використовувати CSS class `.widget-pending`, `.widget-success`, `.widget-error`

## Файли

| Файл | Дія |
|------|-----|
| `webui/src/lib/settings.js` | СТВОРИТИ |
| `webui/src/components/widgets/NumberInput.svelte` | РЕДАГУВАТИ |
| `webui/src/components/widgets/ToggleWidget.svelte` | РЕДАГУВАТИ |
| `webui/src/components/widgets/SelectWidget.svelte` | РЕДАГУВАТИ |
| `webui/src/components/widgets/SliderWidget.svelte` | ВЕРИФІКУВАТИ (debounce з 1.2b) |

## Критерії завершення

- [ ] `settings.js` створено з debounce + optimistic update + error rollback
- [ ] NumberInput: клік +/- 5 разів → 1 POST (debounce 500ms)
- [ ] ToggleWidget: pending state visible, error rollback
- [ ] SelectWidget: pending state visible, error rollback
- [ ] `npm run build` < 55KB gz
- [ ] Manual test: зміна параметра → success flash → value зберігся

## Після завершення

```bash
cd webui && npm run build && npm run deploy
git add webui/src/lib/settings.js webui/src/components/widgets/
git commit -m "feat(webui): widget debounce + loading states — settings.js utility

- createSettingSender: debounce, optimistic update, error rollback
- NumberInput: 500ms debounce (was instant per-click)
- ToggleWidget: pending/error states
- SelectWidget: pending/error states"
git push origin main
```
