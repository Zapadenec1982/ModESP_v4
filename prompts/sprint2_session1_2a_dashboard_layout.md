# Sprint 2 / Сесія 1.2a — Dashboard Layout + Information Hierarchy

## Контекст

Sprint 1 завершено: Design System tokens створено, delta WS broadcasts працюють.
Dashboard — головна сторінка (90% часу інженера). Зараз:
- Температура величезна (64px) але аварії/стан — дрібні іконки, легко пропустити
- Equipment status: SVG іконки 28px без тексту — незрозуміло
- Alarm banner НИЖЧЕ температури — користувач може не помітити
- Компресор зелений (#22c55e) — конфліктує з "success" green

## Задачі

### 1. Переструктурувати Dashboard Layout

Файл: `webui/src/pages/Dashboard.svelte`

**Новий порядок (зверху вниз):**

1. **Alarm Banner** (першим! зараз рядок 115-119, перенести ВГОРУ)
   - Full width, `--alarm-bg` + `--alarm-border`
   - Пульсуюча анімація (залишити)
   - Tap → навігація до Protection page
   - i18n текст аварії

2. **Main Temperature Card**
   - Hero число: `--text-hero` (56px mobile, 72px desktop)
   - Color-coded за відхиленням від setpoint (існуюча логіка)
   - Setpoint inline під температурою: "SP: -20.0°C" (`--text-2xl`)
   - Slider прямо під setpoint (не в окремому блоці)

3. **Equipment Status Bar** — **КЛЮЧОВА ЗМІНА**
   - Замінити SVG іконки на **Pills з текстом**:
     - `[КОМП ВКЛ]` — background `--status-compressor` (#2563eb синій)
     - `[ВЕНТ ВКЛ]` — background `--status-fan` (#0891b2 ціан)
     - `[ВІДТАЙКА]` — background `--status-defrost` (#f59e0b amber)
     - `[ДВЕРІ]` — background `--alarm-border` (#ef4444 червоний) якщо відкриті
   - Pills OFF стан: outline border, muted text
   - Горизонтальний flex row з wrap
   - Statetext під pills: "ОХОЛОДЖЕННЯ" / "ПРОСТІЙ" / "ВІДТАЙКА ФАЗА 3" тощо

4. **Secondary Temperatures** (нова секція)
   - Видима тільки коли `equipment.has_evap_temp` або `equipment.has_cond_temp` = true
   - Compact row: "Випарник: -25.3°C | Конденсатор: 38.1°C"
   - Font size `--text-lg`

5. **MiniChart** (залишити внизу)

### 2. Equipment Pills Component

Створити або вбудувати в Dashboard:
- Pill = `<span class="pill {active ? 'active' : ''}">` з текстом
- Active: filled background + white text
- Inactive: transparent bg + border + muted text
- Min-height: `--touch-min` (44px)
- i18n підтримка (keys: dash.comp, dash.fan, dash.heat, dash.door)

### 3. Night Mode + Defrost Badges

Поточні badges (рядок ~110): ідентичний стиль для Night та Defrost.
- Night: синьо-фіолетовий badge з іконкою місяця
- Defrost: amber badge
- Розмістити поруч з equipment pills або під ними

## Файли

| Файл | Дія |
|------|-----|
| `webui/src/pages/Dashboard.svelte` | ПОВНИЙ РЕФАКТОРИНГ |
| `webui/src/stores/i18n.js` або `i18n/uk.js` + `en.js` | Додати нові keys якщо потрібно |

## Критерії завершення

- [ ] Alarm banner ПЕРШИМ елементом на сторінці
- [ ] Equipment status: pills з текстом (не іконки)
- [ ] Компресор — СИНІЙ (не зелений)
- [ ] Вторинні температури відображаються при наявності has_* keys
- [ ] Mobile: все readable, pills wrap на вузькому екрані
- [ ] `npm run build` проходить, bundle < 55KB gz
- [ ] i18n: UA та EN тексти для всіх нових елементів

## Після завершення

```bash
cd webui && npm run build && npm run deploy
git add webui/src/pages/Dashboard.svelte webui/src/i18n/
git commit -m "feat(webui): dashboard redesign — alarm first, equipment pills, secondary temps

- Alarm banner moved to top (was below temperature)
- Equipment status: text pills instead of SVG icons
- Compressor: blue (#2563eb) instead of green
- Secondary temperatures (evap/cond) when hardware bound
- Mobile-first responsive layout"
git push origin main
```
