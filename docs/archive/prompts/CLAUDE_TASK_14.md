# CLAUDE_TASK: Phase 14 — DataLogger + Temperature Chart

> Логування температури та подій обладнання з відображенням графіків у WebUI.
> Діагностичний інструмент для пусконаладки на місці — без зовнішнього сервера.

---

## Контекст

При діагностиці холодильного обладнання на місці потрібно бачити:
- Криву температури камери за останні 24-48 годин ("пилка" охолодження)
- Моменти вмикання/вимикання компресора
- Зони дефросту (початок, кінець, тип)
- Аварії та їх тривалість

Зараз ці дані доступні тільки через MQTT на зовнішній сервер (Grafana/InfluxDB).
DataLogger дає автономний графік прямо в WebUI контролера.

**Оцінка трудовитрат: 5-6 годин (2 сесії).**

---

## Ресурси (перевірено)

| Ресурс | Зайнято | Ліміт | DataLogger потребує | Запас |
|--------|---------|-------|---------------------|-------|
| LittleFS | ~90 KB | 384 KB | ~16 KB (48 год) | 294 KB вільно |
| HTTP handlers | 37 | 48 | +2 | 9 вільно |
| SharedState keys | 84 | 128 | +6 | 38 вільно |
| ModuleManager | 13 | 16 | +1 | 2 вільно |
| Bundle size | ~30 KB gz | — | +2-3 KB | некритично |

---

## Архітектура

### Потік даних

```
SharedState (equipment.air_temp, equipment.compressor, ...)
        |
        v
   DataLogger module (LOW priority, polling кожен тік)
        |
        +---> RAM buffer (до flush)
        |         |
        |         v (flush кожні 10 хв)
        |    LittleFS: append-only файли + rotate
        |
        v
   GET /api/log -> streaming chunked JSON -> ChartWidget (SVG)
```

### Зберігання: Append + Rotate (не ring buffer)

**Чому не ring buffer:** Ring buffer в файлі потребує точного head/tail tracking,
wrap-around при читанні (два fread), і вразливий до power loss під час запису meta.
Append + rotate значно простіше і стійкіше.

**Механізм:**
1. Нові записи завжди **append** в кінець файлу (`fseek(SEEK_END)` + `fwrite`)
2. Коли файл перевищує ліміт → `rename("temp.bin", "temp.old")` + створити новий `temp.bin`
3. Читання: `temp.old` (якщо є) + `temp.bin` = повна історія
4. Retention: просто видалити `temp.old` коли він старший за retention_hours
5. **Power-loss safe:** навіть якщо живлення зникне під час запису — втратиться максимум один flush (10 хв даних). Файли не пошкоджуються, бо LittleFS — CoW (copy-on-write).

**Без meta.bin:** позиція не потрібна — завжди append, завжди читаємо весь файл.
Час першого запису = перший `TempRecord.minute_offset`. Початок епохи зберігається
як перший запис у файлі з absolute timestamp.

### RAM buffer

10 хвилин між flush:
- 10 записів температури × 8 bytes = 80 bytes
- ~20 подій × 8 bytes = 160 bytes
- Разом: ~240 bytes — мізер

### Файлова структура LittleFS

```
/data/log/
    header.bin     -- 8 bytes: epoch старту логу (int64)
    temp.bin       -- append-only, поточний
    temp.old       -- попередній (після rotate)
    events.bin     -- append-only, поточний
    events.old     -- попередній (після rotate)
```

**Ліміти файлів:**
- temp.bin max = `retention_hours * 60 * sizeof(TempRecord)` = 48*60*8 = 23 KB
- events.bin max = 16 KB (hard limit)
- Разом з .old: ~78 KB максимум

---

## Формат даних (компактний бінарний)

**Запис температури (8 bytes):**
```c
struct TempRecord {
    uint32_t timestamp;    // UNIX epoch (секунди) або uptime_sec якщо NTP недоступний
    int16_t  temp_x10;     // температура × 10 (-345 = -34.5°C)
    uint16_t _reserved;    // alignment padding (або evap_temp_x10 для майбутнього)
};
```

**Запис події (8 bytes):**
```c
struct EventRecord {
    uint32_t timestamp;    // UNIX epoch або uptime_sec
    uint8_t  event_type;   // тип події (enum)
    uint8_t  _pad[3];      // alignment
};
```

Обидва записи 8 bytes — aligned, ефективний fread/fwrite масивами.

**Типи подій:**
```c
enum EventType : uint8_t {
    COMPRESSOR_ON   = 1,
    COMPRESSOR_OFF  = 2,
    DEFROST_START   = 3,
    DEFROST_END     = 4,
    ALARM_HIGH      = 5,
    ALARM_LOW       = 6,
    ALARM_CLEAR     = 7,
    DOOR_OPEN       = 8,
    DOOR_CLOSE      = 9,
    POWER_ON        = 10,
};
```

**Розмір за 24 години:**
- Температура: 1440 × 8 = 11.5 KB
- Події: ~500 × 8 = 4 KB
- Разом: ~16 KB / добу. 48 годин: ~32 KB

**Flash wear:**
- Flush кожні 10 хв = 144 flush/добу
- Кожен flush — append, не rewrite. LittleFS CoW = ~1 sector erase per flush
- 1 сектор: ~100K циклів / 144 = ~694 днів без wear leveling
- З wear leveling (~350 KB вільно / 4 KB сектор = ~87 секторів): **694 × 87 ÷ кількість активних секторів ≈ роки**
- **Достатньо для production.** Але якщо потрібен запас — збільшити flush інтервал до 30 хв (48 flush/добу).

---

## DataLogger Module (C++)

### Розташування

```
modules/datalogger/
    manifest.json
    include/datalogger_module.h
    src/datalogger_module.cpp
    CMakeLists.txt
```

Окремий ESP-IDF component в `modules/` (як thermostat, defrost, protection).
Пріоритет LOW(3) — не впливає на бізнес-логіку.

### CMakeLists.txt

```cmake
idf_component_register(
    SRCS "src/datalogger_module.cpp"
    INCLUDE_DIRS "include"
    REQUIRES modesp_core
)
```

### Клас

```cpp
class DataLoggerModule : public modesp::BaseModule {
public:
    DataLoggerModule();
    bool on_init() override;
    void on_update(uint32_t dt_ms) override;
    void on_stop() override;

    // HTTP handler викликає це для серіалізації
    bool serialize_log_chunked(httpd_req_t* req, int hours) const;
    bool serialize_summary(char* buf, size_t buf_size) const;

private:
    // RAM буфер
    etl::vector<TempRecord, 16>   temp_buf_;
    etl::vector<EventRecord, 32>  event_buf_;

    // Стан polling
    bool prev_compressor_     = false;
    bool prev_defrost_active_ = false;
    bool prev_door_open_      = false;
    bool prev_alarm_high_     = false;
    bool prev_alarm_low_      = false;

    // Таймери
    uint32_t sample_timer_ms_ = 0;
    uint32_t flush_timer_ms_  = 0;

    // Файлові операції
    bool flush_to_flash();
    void rotate_if_needed(const char* path, size_t max_size);
    void log_event(EventType type);
    uint32_t current_timestamp() const;  // epoch або uptime

    // Статистика
    uint32_t temp_count_  = 0;
    uint32_t event_count_ = 0;
};
```

### Логіка on_update()

```cpp
void DataLoggerModule::on_update(uint32_t dt_ms) {
    if (!read_bool("datalogger.enabled", true)) return;

    int sample_sec = read_int("datalogger.sample_interval", 60);

    // 1. Семплювання температури
    sample_timer_ms_ += dt_ms;
    if (sample_timer_ms_ >= (uint32_t)sample_sec * 1000) {
        sample_timer_ms_ = 0;
        float temp = read_float("equipment.air_temp", 0.0f);
        TempRecord rec = { current_timestamp(), (int16_t)(temp * 10), 0 };
        if (!temp_buf_.full()) temp_buf_.push_back(rec);
    }

    // 2. Polling подій (кожен тік — дешево, тільки read_bool)
    bool comp = read_bool("equipment.compressor", false);
    if (comp != prev_compressor_) {
        log_event(comp ? COMPRESSOR_ON : COMPRESSOR_OFF);
        prev_compressor_ = comp;
    }
    // аналогічно: defrost.active, protection.alarm_high, protection.alarm_low, equipment.door_open

    // 3. Flush на LittleFS кожні 10 хвилин
    flush_timer_ms_ += dt_ms;
    if (flush_timer_ms_ >= 600000) {  // 10 хв
        flush_timer_ms_ = 0;
        flush_to_flash();
    }
}
```

### Логіка flush_to_flash()

```cpp
bool DataLoggerModule::flush_to_flash() {
    if (temp_buf_.empty() && event_buf_.empty()) return true;

    // Append температури
    if (!temp_buf_.empty()) {
        FILE* f = fopen("/data/log/temp.bin", "ab");  // append binary
        if (f) {
            fwrite(temp_buf_.data(), sizeof(TempRecord), temp_buf_.size(), f);
            fclose(f);
            temp_count_ += temp_buf_.size();
            temp_buf_.clear();
        }
        int retention = read_int("datalogger.retention_hours", 48);
        size_t max_size = retention * 60 * sizeof(TempRecord);
        rotate_if_needed("/data/log/temp.bin", max_size);
    }

    // Append подій
    if (!event_buf_.empty()) {
        FILE* f = fopen("/data/log/events.bin", "ab");
        if (f) {
            fwrite(event_buf_.data(), sizeof(EventRecord), event_buf_.size(), f);
            fclose(f);
            event_count_ += event_buf_.size();
            event_buf_.clear();
        }
        rotate_if_needed("/data/log/events.bin", 16384);
    }

    // Оновити статистику в SharedState
    state_set("datalogger.records_count", (int32_t)temp_count_);
    state_set("datalogger.events_count", (int32_t)event_count_);
    return true;
}
```

### Логіка rotate_if_needed()

```cpp
void DataLoggerModule::rotate_if_needed(const char* path, size_t max_size) {
    struct stat st;
    if (stat(path, &st) != 0) return;
    if ((size_t)st.st_size <= max_size) return;

    // path.bin -> path.old (перезапише старий .old)
    char old_path[48];
    snprintf(old_path, sizeof(old_path), "%s.old",
             etl::string_view(path, strlen(path) - 4).data());  // замінити .bin на .old
    // Простіше: hardcoded шляхи
    if (strcmp(path, "/data/log/temp.bin") == 0) {
        remove("/data/log/temp.old");
        rename("/data/log/temp.bin", "/data/log/temp.old");
    } else if (strcmp(path, "/data/log/events.bin") == 0) {
        remove("/data/log/events.old");
        rename("/data/log/events.bin", "/data/log/events.old");
    }
    ESP_LOGI(TAG, "Rotated %s (was %lu bytes)", path, (unsigned long)st.st_size);
}
```

### Boot (on_init)

```cpp
bool DataLoggerModule::on_init() {
    // Створити директорію (якщо не існує)
    mkdir("/data/log", 0775);

    // Порахувати існуючі записи (відновлення після ребуту)
    struct stat st;
    if (stat("/data/log/temp.bin", &st) == 0)
        temp_count_ = st.st_size / sizeof(TempRecord);
    if (stat("/data/log/temp.old", &st) == 0)
        temp_count_ += st.st_size / sizeof(TempRecord);
    // Аналогічно events

    // POWER_ON маркер
    log_event(POWER_ON);

    ESP_LOGI(TAG, "DataLogger: %lu temp, %lu events",
             (unsigned long)temp_count_, (unsigned long)event_count_);
    return true;
}
```

---

## HTTP API

### GET /api/log?hours=24

**Streaming chunked JSON** — серіалізується порціями по 512 bytes,
без великого буфера в heap. Патерн вже є в http_service.cpp (static file serving).

```cpp
static esp_err_t handle_get_log(httpd_req_t* req) {
    // Парсимо ?hours=24
    int hours = 24;
    // ... parse query param ...

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    // Делегуємо DataLogger модулю
    return data_logger.serialize_log_chunked(req, hours);
}
```

**serialize_log_chunked() — стратегія:**
```cpp
bool DataLoggerModule::serialize_log_chunked(httpd_req_t* req, int hours) const {
    char buf[512];
    int len;

    // 1. Header
    len = snprintf(buf, sizeof(buf), "{\"temp\":[");
    httpd_resp_send_chunk(req, buf, len);

    // 2. Читаємо temp.old + temp.bin порціями
    const char* files[] = {"/data/log/temp.old", "/data/log/temp.bin"};
    bool first = true;
    uint32_t cutoff = current_timestamp() - hours * 3600;

    for (auto path : files) {
        FILE* f = fopen(path, "rb");
        if (!f) continue;
        TempRecord rec;
        while (fread(&rec, sizeof(rec), 1, f) == 1) {
            if (rec.timestamp < cutoff) continue;  // skip old
            len = snprintf(buf, sizeof(buf), "%s[%lu,%d]",
                          first ? "" : ",",
                          (unsigned long)rec.timestamp,
                          rec.temp_x10);
            httpd_resp_send_chunk(req, buf, len);
            first = false;
        }
        fclose(f);
    }

    // 3. Events section
    len = snprintf(buf, sizeof(buf), "],\"events\":[");
    httpd_resp_send_chunk(req, buf, len);

    // ... аналогічно events.old + events.bin ...

    // 4. Footer
    httpd_resp_send_chunk(req, "]}", 2);
    httpd_resp_send_chunk(req, nullptr, 0);  // end chunked
    return true;
}
```

**Формат відповіді:**
```json
{
    "temp": [
        [1740000060, -345],
        [1740000120, -342]
    ],
    "events": [
        [1740000000, 10],
        [1740000300, 1]
    ]
}
```

Максимально компактний: масив масивів, без ключів.
- `temp`: `[timestamp, temp_x10]`
- `events`: `[timestamp, event_type]`

**Розмір (24 год):**
- 1440 temp × ~20 bytes = ~28 KB
- ~500 events × ~16 bytes = ~8 KB
- Разом: ~36 KB (streaming, без буферу в RAM)

### GET /api/log/summary

Легкий endpoint (< 256 bytes відповідь):
```json
{
    "hours": 36,
    "temp_count": 2160,
    "event_count": 823,
    "flash_kb": 28
}
```

---

## WebUI — ChartWidget (SVG)

### Файл

```
webui/src/components/widgets/ChartWidget.svelte — ~200-250 рядків
```

Чистий SVG в Svelte — нуль зовнішніх бібліотек.

### Як widget отримує дані

ChartWidget — **нестандартний widget.** Звичайні widgets отримують `value` (скаляр з SharedState).
ChartWidget робить `apiGet()` в `onMount()` + при зміні періоду.

**Прецедент:** `WifiScan.svelte`, `MqttSave.svelte`, `TimeSave.svelte` — вже роблять `apiGet()` в `onMount()`.

```svelte
<script>
  import { onMount } from 'svelte';
  import { apiGet } from '../../lib/api.js';

  export let config;   // { key, widget, data_source, default_hours }
  export let value;     // не використовується — дані з API

  let data = null;
  let loading = true;
  let hours = config.default_hours || 24;

  async function loadData() {
    loading = true;
    try {
      data = await apiGet(`${config.data_source}?hours=${hours}`);
    } catch (e) { data = null; }
    loading = false;
  }

  onMount(loadData);
  // Auto-refresh кожні 5 хвилин
  onMount(() => {
    const iv = setInterval(loadData, 300000);
    return () => clearInterval(iv);
  });
</script>
```

### SVG Downsampling

1440 точок за 24 год — ОК на десктопі, може гальмувати на дешевому мобільному.

**Рішення: Largest-Triangle-Three-Buckets (LTTB)** — зберігає форму кривої при зменшенні точок.
Або простіший варіант — min/max per bucket (зберігає піки):

```javascript
function downsample(points, maxPoints) {
    if (points.length <= maxPoints) return points;
    const bucket = Math.ceil(points.length / maxPoints);
    const result = [];
    for (let i = 0; i < points.length; i += bucket) {
        const slice = points.slice(i, i + bucket);
        // Зберегти min і max в кожному bucket (щоб піки не зникали)
        const min = slice.reduce((a, b) => a[1] < b[1] ? a : b);
        const max = slice.reduce((a, b) => a[1] > b[1] ? a : b);
        if (min[0] < max[0]) { result.push(min, max); }
        else { result.push(max, min); }
    }
    return result;
}
```

Максимум точок в SVG: **720** (достатньо для плавної кривої на будь-якому екрані).

### Візуалізація

```
+--------------------------------------------------+
|  Температура камери              24h  48h         |  <- перемикач періоду
|                                                   |
|  -30°C |                                          |
|        |    /\    /\    /\    /\    /\             |  <- крива температури
|  -33°C |/\/  \/\/  \/\/  \/\/  \/\/  \/           |     (синя лінія)
|        |                                          |
|  -35°C |- - - - - - - - - - - - - - - - - - -     |  <- setpoint (пунктир)
|        |  |||  |||  |||  |||  |||  |||             |  <- компресор ON (зелені)
|        |         %%%                    %%%        |  <- дефрост (оранжеві)
|        +----+----+----+----+----+----+----+       |
|        00   04   08   12   16   20   00           |  <- вісь часу
|                                                   |
|  * -33.2°C  15:42  comp: ON                      |  <- tooltip при hover
+--------------------------------------------------+
```

### Елементи SVG

1. **Крива температури** — `<polyline>` (після downsampling — max 720 точок)
2. **Лінія setpoint** — `<line>` пунктирна
3. **Зони компресора** — `<rect>` зелені напівпрозорі (будуються з пар ON/OFF подій)
4. **Зони дефросту** — `<rect>` оранжеві
5. **Маркери аварій** — `<line>` червоні вертикальні
6. **Маркери POWER_ON** — `<line>` сірі вертикальні (ребут)
7. **Сітка** — `<line>` + `<text>` осі часу та температури
8. **Tooltip** — `<g>` при hover/touch з найближчою точкою

### Стилі

```css
.temp-line { fill: none; stroke: #3b82f6; stroke-width: 1.5; }
.setpoint { stroke: #f59e0b; stroke-width: 1; stroke-dasharray: 4 2; }
.zone-comp { fill: #22c55e; opacity: 0.15; }
.zone-defrost { fill: #f97316; opacity: 0.2; }
.grid { stroke: var(--border); stroke-width: 0.5; }
.axis-label { font-size: 10px; fill: var(--fg-muted); }
```

### Допоміжна функція: buildZones

```javascript
function buildZones(events, onType, offType, width, totalMinutes) {
    const zones = [];
    let start = null;
    for (const [ts, type] of events) {
        if (type === onType) start = ts;
        else if (type === offType && start !== null) {
            zones.push({
                x1: tsToX(start, width, totalMinutes),
                x2: tsToX(ts, width, totalMinutes)
            });
            start = null;
        }
    }
    // Незакрита зона — тягнеться до кінця
    if (start !== null) zones.push({ x1: tsToX(start, width, totalMinutes), x2: width });
    return zones;
}
```

---

## Маніфест

**Файл:** `modules/datalogger/manifest.json`

```json
{
  "manifest_version": 1,
  "module": "datalogger",
  "description": "Temperature and event logger with LittleFS storage",

  "state": {
    "datalogger.enabled": {
      "type": "bool", "access": "readwrite", "default": true,
      "persist": true, "description": "Логування увімкнено"
    },
    "datalogger.retention_hours": {
      "type": "int", "access": "readwrite", "persist": true,
      "min": 12, "max": 168, "step": 12, "default": 48,
      "description": "Глибина зберігання (год)"
    },
    "datalogger.sample_interval": {
      "type": "int", "access": "readwrite", "persist": true,
      "min": 30, "max": 300, "step": 30, "default": 60,
      "description": "Інтервал семплювання (с)"
    },
    "datalogger.records_count": {
      "type": "int", "access": "read",
      "description": "Записів температури"
    },
    "datalogger.events_count": {
      "type": "int", "access": "read",
      "description": "Подій"
    },
    "datalogger.flash_used": {
      "type": "int", "access": "read",
      "description": "Flash (KB)"
    }
  },

  "ui": {
    "page": "Графік",
    "page_id": "chart",
    "icon": "chart",
    "order": 3,
    "cards": [
      {
        "title": "Температура",
        "widgets": [
          {
            "key": "datalogger.chart",
            "widget": "chart",
            "data_source": "/api/log",
            "default_hours": 24
          }
        ]
      },
      {
        "title": "Статистика",
        "widgets": [
          {"key": "datalogger.records_count", "widget": "value", "label": "Записів"},
          {"key": "datalogger.events_count",  "widget": "value", "label": "Подій"},
          {"key": "datalogger.flash_used",    "widget": "value", "label": "Flash (KB)"}
        ]
      },
      {
        "title": "Налаштування",
        "collapsible": true,
        "widgets": [
          {"key": "datalogger.enabled",         "widget": "toggle"},
          {"key": "datalogger.retention_hours",  "widget": "number_input"},
          {"key": "datalogger.sample_interval",  "widget": "number_input"}
        ]
      }
    ]
  }
}
```

---

## Інтеграція

### main.cpp

```cpp
#include "datalogger_module.h"

static DataLoggerModule  datalogger;

// Після defrost, перед Phase 3 (HTTP):
app.modules().register_module(datalogger);
```

DataLogger реєструється в Phase 2 разом з бізнес-модулями (пріоритет LOW,
виконується ПІСЛЯ equipment/protection/thermostat/defrost).

### http_service.cpp

Додати 2 endpoints в масив `api_handlers[]`:
```cpp
{"/api/log",         HTTP_GET, handle_get_log},
{"/api/log/summary", HTTP_GET, handle_get_log_summary},
```

Потрібна ін'єкція `DataLoggerModule*`:
```cpp
http_service.set_datalogger(&datalogger);
```

### WidgetRenderer.svelte

```javascript
import ChartWidget from './widgets/ChartWidget.svelte';
// В widgetMap:
chart: ChartWidget,
```

### project.json

Додати `"datalogger"` в список модулів.

---

## Файли

### Створити (6 файлів)

| Файл | Рядків (~) |
|------|------------|
| `modules/datalogger/manifest.json` | 50 |
| `modules/datalogger/include/datalogger_module.h` | 60 |
| `modules/datalogger/src/datalogger_module.cpp` | 250 |
| `modules/datalogger/CMakeLists.txt` | 6 |
| `webui/src/components/widgets/ChartWidget.svelte` | 230 |
| `webui/src/lib/downsample.js` | 20 |

### Змінити (4 файли)

| Файл | Зміни |
|------|-------|
| `main/main.cpp` | +4 рядки (include, static, register, set_datalogger) |
| `components/modesp_net/src/http_service.cpp` | +2 endpoints + handler functions (~60 рядків) |
| `webui/src/components/WidgetRenderer.svelte` | +2 рядки (import + map entry) |
| `project.json` | +1 рядок |

**Разом: ~680 рядків нового коду.**

---

## Порядок реалізації

### Сесія A (~3 год): Backend

1. **DataLogger module** — header, CMakeLists, manifest
2. **on_init()** — mkdir, count existing records, POWER_ON event
3. **on_update()** — sample timer, event polling, flush timer
4. **flush_to_flash()** — append to files, rotate_if_needed()
5. **serialize_log_chunked()** — streaming JSON з fread порціями
6. **main.cpp** — register module
7. **http_service.cpp** — endpoints + set_datalogger()
8. **Verify:** `idf.py build`, flash, перевірити в serial monitor що записи з'являються

### Сесія B (~2-3 год): Frontend

1. **downsample.js** — min/max bucket downsampling
2. **ChartWidget.svelte** — SVG з temperature line, zones, tooltip
3. **WidgetRenderer** — register chart type
4. **generate_ui.py + build + deploy**
5. **Verify:** відкрити WebUI → сторінка "Графік" → крива температури, зони

---

## Критерій завершення

**Backend:**
- [ ] Температура логується кожні `sample_interval` секунд (default 60)
- [ ] Події (компресор, дефрост, аварії, двері) логуються миттєво при зміні
- [ ] RAM buffer flush на LittleFS кожні 10 хвилин (append, не rewrite)
- [ ] Rotate при перевищенні ліміту файлу
- [ ] Дані переживають ребут (on_init відновлює count)
- [ ] POWER_ON маркер при кожному boot
- [ ] GET /api/log — streaming chunked JSON, не тримає весь response в RAM
- [ ] GET /api/log/summary — легкий endpoint < 256 bytes
- [ ] pytest green, idf.py build без помилок

**Frontend:**
- [ ] ChartWidget відображає криву температури (SVG polyline)
- [ ] Downsampling до max 720 точок (min/max bucket)
- [ ] Зони компресора (зелені) та дефросту (оранжеві)
- [ ] Setpoint пунктирна лінія
- [ ] Tooltip при hover/touch
- [ ] Перемикач 24h / 48h
- [ ] Auto-refresh кожні 5 хвилин
- [ ] Loading state при завантаженні даних
- [ ] Bundle size зріс не більше ніж на 3 KB gzipped

---

## Changelog

- 2026-02-24 — Оптимізовано: ring buffer → append + rotate (простіше, power-loss safe).
  Видалено meta.bin (не потрібен). Записи 8 bytes aligned (було 4/5). Streaming chunked JSON
  для HTTP response (не один буфер). SVG downsampling до 720 точок (min/max bucket).
  Модуль в modules/datalogger/ (було modesp_services). Правильний manifest format ("ui": {...}).
  Додано оцінку часу (5-6 год, 2 сесії). Flash wear розрахунок виправлено.
- 2026-02-23 — Створено.
