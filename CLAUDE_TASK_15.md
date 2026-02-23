# CLAUDE_TASK: Phase 15 — i18n (Internationalization)

> Мультимовний інтерфейс WebUI: українська, англійська, польська.
> Мова зберігається в NVS, перемикається з інтерфейсу.

---

## Контекст

Контролер ModESP використовується в Україні, Польщі та потенційно інших країнах.
WebUI має відображатись мовою зрозумілою оператору обладнання.
Зараз всі тексти — суміш українською (маніфести) та англійською (Svelte компоненти).

---

## Джерела текстів (інвентаризація)

### 1. Маніфести → ui.json (генеруються)
Тексти які потрапляють через generate_ui.py в ui.json:
- `description` — опис state key ("Уставка", "Диференціал", "Тип розморозки")
- `label` — мітка віджету ("Аварія", "Код аварії")
- `on_label` / `off_label` — ("АВАРІЯ" / "Норма", "ТАК" / "НІ")
- `options[].label` — опції select ("За часом", "Електрична", "Гарячий газ")
- Card `title` — ("Датчики", "Захист", "Налаштування")
- Page `title` — ("Dashboard", "Термостат", "Розморозка", "Мережа")
- `disabled_hint` — ("Потрібен тен (heater)")
- `disabled_reason` — ("Потрібно: evap_temp")
- `unit` — ("°C", "хв", "с") — одиниці вимірювання, мовонезалежні

### 2. Svelte компоненти (захардкоджені)
Рядки прямо в .svelte файлах:
- Layout.svelte: "Online", "Offline", "ALARM: ..."
- Dashboard.svelte: "Компресор", "NIGHT", температурні підписи
- BindingsEditor.svelte: "Save", "Saving...", "Scan", "Restart",
  "Restarting...", "OneWire Discovery", "No new devices"
- WifiScan.svelte: "Сканування...", "Сканувати"
- WifiSave.svelte: "Save"
- TimeSave.svelte: "Save"
- SelectWidget.svelte: "⊘" символ для disabled

### 3. Системні сторінки (generate_ui.py)
Генератор створює системні сторінки з текстами:
- "WiFi", "Налаштування WiFi", "Мережа"
- "MQTT", "Налаштування MQTT"
- "Система", "Firmware", "OTA оновлення"
- Descriptions системних ключів ("Мережа", "IP адреса", "Сигнал")

---

## Архітектура i18n

### Підхід: словник на клієнті + маніфести з ключами

**Принцип:** маніфести зберігають не тексти, а ключі перекладу.
Словник завантажується один раз при старті WebUI.

### Два типи перекладу

**Тип A — Статичні рядки Svelte:**
Словник `i18n/{lang}.json` з ключами типу `ui.online`, `ui.save`, `ui.alarm`.

**Тип B — Маніфестні тексти:**
Маніфести містять мовні варіанти прямо в полях:

```json
{
  "key": "thermostat.setpoint",
  "description": {
    "uk": "Уставка",
    "en": "Setpoint",
    "pl": "Nastawa"
  }
}
```

Або альтернативно — маніфести зберігають тільки ключ,
а переклади в окремому словнику модуля.

### Рекомендований підхід: мовні варіанти в маніфестах

Переваги:
- Single source of truth залишається — маніфест описує і логіку і тексти
- Не потрібен окремий файл перекладів для кожного модуля
- generate_ui.py вибирає потрібну мову при генерації ui.json
- Або передає ВСІ мови в ui.json, а WebUI вибирає потрібну runtime

### Формат мовних полів в маніфесті

Кожне текстове поле може бути string АБО object з мовними ключами:

```json
"thermostat.setpoint": {
  "type": "float",
  "description": {
    "uk": "Уставка",
    "en": "Setpoint",
    "pl": "Nastawa"
  },
  "min": -40, "max": 10, "step": 0.5
}
```

```json
"options": [
  {"value": 0, "label": {"uk": "Вимкнено", "en": "Off", "pl": "Wyłączony"}},
  {"value": 1, "label": {"uk": "Розклад (SNTP)", "en": "Schedule (SNTP)", "pl": "Harmonogram (SNTP)"}},
  {"value": 2, "label": {"uk": "Зовнішній вхід (DI)", "en": "External input (DI)", "pl": "Wejście zewnętrzne (DI)"}}
]
```

```json
"ui": {
  "page": {"uk": "Термостат", "en": "Thermostat", "pl": "Termostat"},
  "cards": [
    {
      "title": {"uk": "Основне", "en": "Main", "pl": "Główne"},
      "widgets": [...]
    }
  ]
}
```

Backward compatible: якщо поле — string → це fallback для всіх мов.

---

## Зберігання мови

Тільки `localStorage` в браузері. Мова інтерфейсу — це налаштування клієнта,
не контролера. ESP32 не потрібно знати якою мовою відображається UI.

Переваги:
- Кожен оператор має свою мову на своєму пристрої
- Не витрачаємо NVS, state key, API endpoint
- Миттєве завантаження — localStorage читається синхронно при старті
- Мова за замовчуванням: `navigator.language` → автовизначення (uk/en/pl),
  fallback на 'en' якщо мова браузера не підтримується

---

## generate_ui.py — два варіанти

### Варіант A: генерувати ui.json для однієї мови (compile-time)

```bash
python generate_ui.py --lang uk
```

ui.json містить тільки обрану мову — description, label, title як звичайні string.
При зміні мови потрібно перегенерувати ui.json і перезавантажити.

Мінус: перезавантаження при зміні мови.

### Варіант B: всі мови в ui.json (runtime)

ui.json містить всі мовні варіанти:
```json
{
  "key": "thermostat.setpoint",
  "description": {"uk": "Уставка", "en": "Setpoint", "pl": "Nastawa"},
  ...
}
```

WebUI вибирає потрібну мову runtime. Зміна мови — миттєва, без перезавантаження.

Мінус: ui.json більший (~x2-3 для текстових полів). Але тексти — мала частина ui.json,
основний обсяг — числові параметри (min, max, step, options values). Реальне збільшення ~30-40%.

### Рекомендація: Варіант B (runtime)

Зміна мови без ребуту — важливо для UX. Збільшення ui.json на ~30% (з ~15KB до ~20KB gzipped)
прийнятне. Це одноразове завантаження при відкритті WebUI.

---

## Словник для статичних рядків Svelte

Файл `webui/src/lib/i18n.js`:

```javascript
const translations = {
  uk: {
    'ui.online': 'Онлайн',
    'ui.offline': 'Офлайн',
    'ui.save': 'Зберегти',
    'ui.saving': 'Збереження...',
    'ui.scan': 'Сканувати',
    'ui.scanning': 'Сканування...',
    'ui.restart': 'Перезавантажити',
    'ui.restarting': 'Перезавантаження...',
    'ui.alarm': 'АВАРІЯ',
    'ui.normal': 'Норма',
    'ui.compressor': 'Компресор',
    'ui.night': 'НІЧ',
    'ui.no_new_devices': 'Нових пристроїв не знайдено',
    'ui.cancel': 'Скасувати',
    'ui.confirm': 'Підтвердити',
    'ui.settings': 'Налаштування',
    'ui.language': 'Мова',
    // ...
  },
  en: {
    'ui.online': 'Online',
    'ui.offline': 'Offline',
    'ui.save': 'Save',
    'ui.saving': 'Saving...',
    'ui.scan': 'Scan',
    'ui.scanning': 'Scanning...',
    'ui.restart': 'Restart',
    'ui.restarting': 'Restarting...',
    'ui.alarm': 'ALARM',
    'ui.normal': 'Normal',
    'ui.compressor': 'Compressor',
    'ui.night': 'NIGHT',
    'ui.no_new_devices': 'No new devices found',
    'ui.cancel': 'Cancel',
    'ui.confirm': 'Confirm',
    'ui.settings': 'Settings',
    'ui.language': 'Language',
    // ...
  },
  pl: {
    'ui.online': 'Online',
    'ui.offline': 'Offline',
    'ui.save': 'Zapisz',
    'ui.saving': 'Zapisywanie...',
    'ui.scan': 'Skanuj',
    'ui.scanning': 'Skanowanie...',
    'ui.restart': 'Restart',
    'ui.restarting': 'Restartowanie...',
    'ui.alarm': 'ALARM',
    'ui.normal': 'Norma',
    'ui.compressor': 'Sprężarka',
    'ui.night': 'NOC',
    'ui.no_new_devices': 'Nie znaleziono nowych urządzeń',
    'ui.cancel': 'Anuluj',
    'ui.confirm': 'Potwierdź',
    'ui.settings': 'Ustawienia',
    'ui.language': 'Język',
    // ...
  }
};
```

### Svelte store

```javascript
// webui/src/stores/i18n.js
import { writable, derived } from 'svelte/store';

const SUPPORTED = ['uk', 'en', 'pl'];

function detectLang() {
  const saved = localStorage.getItem('lang');
  if (saved && SUPPORTED.includes(saved)) return saved;
  // Автовизначення за мовою браузера
  const nav = (navigator.language || '').slice(0, 2).toLowerCase();
  if (SUPPORTED.includes(nav)) return nav;
  return 'en';
}

export const currentLang = writable(detectLang());

// Зберігаємо в localStorage при зміні
currentLang.subscribe(lang => {
  localStorage.setItem('lang', lang);
});

// Хелпер для перекладу статичних рядків
export const t = derived(currentLang, $lang => {
  return (key) => translations[$lang]?.[key] || translations['en']?.[key] || key;
});

// Хелпер для перекладу маніфестних полів (description, label, title)
export const tr = derived(currentLang, $lang => {
  return (field) => {
    if (!field) return '';
    if (typeof field === 'string') return field;  // backward compat
    return field[$lang] || field['en'] || field['uk'] || Object.values(field)[0] || '';
  };
});
```

### Використання в Svelte компонентах

```svelte
<script>
  import { t, tr } from '../stores/i18n.js';
</script>

<!-- Статичний рядок -->
<span>{$t('ui.online')}</span>

<!-- Маніфестний текст (description може бути string або {uk, en, pl}) -->
<div class="label">{$tr(config.description)}</div>

<!-- Option label -->
{#each options as opt}
  <option>{$tr(opt.label)}</option>
{/each}
```

---

## Перемикач мови в UI

В Layout.svelte — sidebar footer або сторінка System:

```svelte
<script>
  import { currentLang } from '../stores/i18n.js';

  const languages = [
    { code: 'uk', label: 'UA 🇺🇦' },
    { code: 'en', label: 'EN 🇬🇧' },
    { code: 'pl', label: 'PL 🇵🇱' }
  ];
</script>

<div class="lang-switch">
  {#each languages as lang}
    <button
      class:active={$currentLang === lang.code}
      on:click={() => {
        $currentLang = lang.code;
      }}
    >
      {lang.label}
    </button>
  {/each}
</div>
```

Компактні кнопки `UA 🇺🇦 | EN 🇬🇧 | PL 🇵🇱` — в sidebar footer або topbar.

---

## Файли які потрібно створити/змінити

### Нові файли
| Файл | Опис |
|------|------|
| `webui/src/lib/i18n.js` | Словник статичних рядків (uk, en, pl) |
| `webui/src/stores/i18n.js` | Svelte stores: currentLang, t(), tr() |

### Маніфести — додати мовні варіанти
| Файл | Кількість текстових полів |
|------|--------------------------|
| `modules/thermostat/manifest.json` | ~40 (descriptions, labels, options) |
| `modules/defrost/manifest.json` | ~35 |
| `modules/protection/manifest.json` | ~25 |
| `modules/equipment/manifest.json` | ~15 |

### generate_ui.py
| Зміна | Опис |
|-------|------|
| `_build_widget()` | Передавати description/label як object якщо мультимовне |
| `_module_page()` | Передавати card title, page title як object |
| Системні сторінки | Мультимовні тексти для WiFi, MQTT, System |
| Валідація | Перевіряти що всі мови присутні (uk, en обов'язкові, pl опціональна) |

### Svelte компоненти — замінити хардкод на $t() / $tr()
| Файл | Рядків для заміни |
|------|-------------------|
| `Layout.svelte` | ~5 ("Online", "Offline", "ALARM") |
| `Dashboard.svelte` | ~5 ("Компресор", "NIGHT") |
| `BindingsEditor.svelte` | ~15 ("Save", "Scan", "Restart", card titles) |
| `WifiScan.svelte` | ~3 |
| `WifiSave.svelte` | ~2 |
| `TimeSave.svelte` | ~2 |
| `SelectWidget.svelte` | ~2 (description, disabled hints) |
| `WidgetRenderer.svelte` | прокидування $tr для descriptions |
| `DynamicPage.svelte` | card title через $tr |

### Backend C++

Змін на backend не потрібно. Мова — це налаштування браузера, не контролера.

---

## Порядок реалізації

1. **i18n store + словник** — створити stores/i18n.js, lib/i18n.js з автовизначенням мови
2. **Svelte компоненти** — замінити хардкоджені рядки на $t()
3. **Маніфести** — додати мовні варіанти до descriptions, labels, options, titles
4. **generate_ui.py** — прокидувати мультимовні поля в ui.json
5. **Svelte widgets** — використовувати $tr() для маніфестних текстів
6. **Перемикач мови** — в Layout sidebar footer
7. **Тестування** — перемикання uk/en/pl, перевірка всіх сторінок

---

## Оцінка обсягу

- **Словник:** ~60-80 статичних ключів × 3 мови
- **Маніфести:** ~120 текстових полів × 3 мови
- **Svelte:** ~15 файлів, ~40 замін хардкоду
- **ui.json:** збільшення ~30-40% (тексти × 3 мови)
- **Bundle:** +1-2 KB (словник вбудований в JS)
- **Складність:** середня, багато рутинної роботи з текстами

---

## Критерій завершення

- [ ] Перемикач мови UA / EN / PL в інтерфейсі
- [ ] Мова зберігається в NVS і localStorage
- [ ] Зміна мови миттєва — без перезавантаження
- [ ] Всі descriptions, labels, option labels перекладені
- [ ] Всі хардкоджені рядки в Svelte замінені на $t()
- [ ] Dashboard, Thermostat, Defrost, Protection, Equipment — перекладені
- [ ] Системні сторінки (WiFi, MQTT, System) — перекладені
- [ ] BindingsEditor — перекладений
- [ ] Fallback: якщо переклад відсутній → англійська → українська
- [ ] generate_ui.py валідація: попередження якщо мова відсутня
- [ ] Додавання нової мови = додати об'єкт в словник + переклади в маніфестах

---

## Додавання нової мови в майбутньому

1. Додати мовний код в `languages` масив в Layout.svelte
2. Додати переклади в `webui/src/lib/i18n.js` (статичні рядки)
3. Додати переклади в кожному маніфесті (`"de": "..."` поруч з uk/en/pl)
4. Перегенерувати: `idf.py build` → `cd webui && npm run build && npm run deploy`

---

## Changelog
- 2026-02-23 — Створено. i18n для WebUI: uk, en, pl.
