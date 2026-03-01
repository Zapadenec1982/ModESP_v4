# Sprint 2 / Сесія 1.3b — Form Fixes + Error Handling

## Контекст

Widget debounce реалізовано (сесія 1.3a). Тепер фіксимо форми та error handling.

## Задачі

### 1. MqttSave — Svelte Stores замість DOM Queries

Файл: `webui/src/components/widgets/MqttSave.svelte`

**Проблема (рядки 29-37):** DOM queries `document.querySelector([data-widget-key="${key}"] input)`.
Це fragile: якщо widget перерендериться, DOM reference ламається.
Race condition: `setTimeout(50ms)` на рядку ~24 сподівається що DOM готовий.

**Fix:** Створити `webui/src/stores/mqttForm.js` (аналогічно існуючому `wifiForm.js`):

```javascript
import { writable } from 'svelte/store';
export const mqttHost = writable('');
export const mqttPort = writable(1883);
export const mqttUser = writable('');
export const mqttPass = writable('');
export const mqttPrefix = writable('');
```

MqttSave читає початкові значення з $state при onMount, пише через stores.
Save button читає зі stores (не з DOM).

### 2. SelectWidget — Disabled Options Polish

Файл: `webui/src/components/widgets/SelectWidget.svelte`

**Проблема:** Disabled options показують `⊘` символ (рядок ~38), hint жовтий 11px.

**Fix:**
- Disabled options: grey text + `text-decoration: line-through`
- Tooltip (title attribute) з `disabled_hint` текстом
- Прибрати `⊘` символ — не рендериться однаково на всіх пристроях
- Hint під select: якщо поточне значення disabled, показати warning inline

### 3. API Error Enrichment

Файл: `webui/src/lib/api.js`

**Проблема (рядок ~17):** `throw new Error(text || 'POST ${url}: ${r.status}')` — user бачить raw HTTP status.

**Fix:**
```javascript
async function handleError(response, url) {
  const body = await response.text().catch(() => '');
  // ESP32 HTTP service returns plain text error messages
  const message = body || getDefaultMessage(response.status, url);
  throw new Error(message);
}

function getDefaultMessage(status, url) {
  // Map to i18n keys
  const messages = {
    400: $t('error.bad_request'),
    401: $t('error.unauthorized'),
    413: $t('error.too_large'),
    422: $t('error.invalid_value'),
    500: $t('error.server_error'),
  };
  return messages[status] || `${status}: ${url}`;
}
```

Додати i18n keys в `uk.js` та `en.js`:
- error.bad_request / error.unauthorized / error.invalid_value / error.server_error / error.too_large

### 4. Connection Overlay

Файл: `webui/src/components/Layout.svelte`

**Якщо не зроблено в сесії 1b:**
- Таймер 5 секунд після WS disconnect
- Overlay: "З'єднання втрачено. Перепідключення..."
- Spinner + "Спроба N з ..."
- Кнопка "Перепідключити зараз"
- Після reconnect: зелений toast + GET /api/state

## Файли

| Файл | Дія |
|------|-----|
| `webui/src/stores/mqttForm.js` | СТВОРИТИ |
| `webui/src/components/widgets/MqttSave.svelte` | РЕДАГУВАТИ |
| `webui/src/components/widgets/SelectWidget.svelte` | РЕДАГУВАТИ |
| `webui/src/lib/api.js` | РЕДАГУВАТИ |
| `webui/src/i18n/uk.js` | РЕДАГУВАТИ (error keys) |
| `webui/src/i18n/en.js` | РЕДАГУВАТИ (error keys) |
| `webui/src/components/Layout.svelte` | ВЕРИФІКУВАТИ/РЕДАГУВАТИ (overlay) |

## Критерії завершення

- [ ] MqttSave: працює через Svelte stores (не DOM queries)
- [ ] SelectWidget: disabled options grey + strikethrough + tooltip
- [ ] API errors: людські повідомлення замість "POST /api/settings: 422"
- [ ] Connection overlay працює (5с delay → overlay → reconnect → dismiss)
- [ ] i18n: error messages на UA та EN
- [ ] `npm run build` < 55KB gz

## Після завершення

```bash
cd webui && npm run build && npm run deploy
git add webui/src/stores/mqttForm.js webui/src/components/widgets/ \
  webui/src/lib/api.js webui/src/i18n/ webui/src/components/Layout.svelte
git commit -m "fix(webui): MqttSave stores, API error enrichment, SelectWidget disabled polish

- MqttSave: Svelte stores instead of DOM queries (was fragile)
- SelectWidget: grey+strikethrough for disabled options (was ⊘ symbol)
- API errors: parse ESP32 response body, i18n error messages
- Connection overlay: 5s delay, spinner, manual retry button"
git push origin main
```

Оновити ACTION_PLAN.md — Sprint 2 повністю завершено.
