# ModESP v4 — Системні сервіси (modesp_services)

Модулі, потрібні в **кожній** прошивці, незалежно від бізнес-логіки.
Реалізовані як звичайні BaseModule. Реєструються в main.cpp.

## Огляд

| Сервіс | Пріоритет | Функція | RAM |
|--------|-----------|---------|-----|
| ErrorService | CRITICAL | Помилки, Safe Mode | ~2 KB |
| WatchdogService | CRITICAL | Heartbeat модулів | ~0.5 KB |
| ConfigService | HIGH | Завантаження/збереження конфіг | ~1 KB |
| PersistService | LOW | Автозбереження стану в NVS | ~0.5 KB |
| LoggerService | LOW | Ring buffer логів | ~5 KB |
| SystemMonitor | LOW | RAM, uptime, boot reason | ~0.2 KB |

## 1. ErrorService — Обробка помилок

Найважливіший сервіс. Без нього прошивка "мовчки" ламається.

### Рівні помилок

```
INFO     → тільки лог
WARNING  → лог + повідомлення на шину
ERROR    → лог + деградований режим
CRITICAL → лог + спроба перезапуску модуля
FATAL    → Safe Mode (вимкнути актуатори, чекати втручання)
```

### Safe Mode

Для промислового холодильника "продовжити працювати з помилкою" 
гірше ніж "зупинитись і повідомити". Safe Mode:

1. Вимикає всі актуатори (компресор OFF, нагрівач OFF)
2. Залишає сенсори активними (для моніторингу)
3. Публікує MsgSafeMode на шину
4. Логує причину
5. Чекає ручного втручання або перезавантаження

### API

```cpp
class ErrorService : public BaseModule {
public:
    ErrorService() : BaseModule("error", ModulePriority::CRITICAL) {}

    // Модулі повідомляють про помилки
    void report(const char* source, int32_t code,
                ErrorSeverity severity, const char* description);

    // Налаштування реакції на рівень помилки
    void set_policy(ErrorSeverity severity, ErrorAction action);

    // Стан
    size_t error_count() const;
    bool is_safe_mode() const;

    // Останні 16 помилок (для діагностики через WebSocket)
    const etl::vector<ErrorRecord, 16>& history() const;
};
```

## 2. WatchdogService — Моніторинг модулів

**Software watchdog** — перевіряє що модулі "живі".
ModuleManager оновлює `last_update_ms` при кожному update().
WatchdogService перевіряє чи не протерміновано.

```cpp
class WatchdogService : public BaseModule {
public:
    // Таймаути по пріоритету модуля
    struct Timeouts {
        uint32_t critical_ms = 5000;   // 5 сек
        uint32_t high_ms     = 10000;  // 10 сек
        uint32_t normal_ms   = 30000;  // 30 сек
        uint32_t low_ms      = 60000;  // 1 хв
    };
    Timeouts timeouts;

    // Максимум 3 спроби перезапуску, потім — error report
    static constexpr uint8_t MAX_RESTARTS = 3;
};
```

**Два рівні watchdog:**
- Software (WatchdogService) → ловить зависання окремих модулів
- Hardware (ESP32 TWDT в App) → ловить зависання всієї системи

## 3. ConfigService — Конфігурація

Завантаження JSON конфігурації → SharedState при старті.
Збереження змін SharedState → NVS.

```
Старт:
  configs/default.json ──parse──► SharedState
  NVS saved config ────load───► SharedState (overrides defaults)

Runtime:
  WebSocket RPC "set config.setpoint=5" → SharedState → mark dirty

Periodic / on demand:
  SharedState (dirty keys) ──save──► NVS
```

### API

```cpp
class ConfigService : public BaseModule {
public:
    bool load(const char* path = "/configs/default.json");
    bool save();
    bool reset_to_defaults(const char* section = nullptr);
    bool export_json(char* buffer, size_t size);
    bool import_json(const char* json_str);
    bool is_dirty() const;
};
```

## 4. PersistService — Автозбереження стану

Зберігає **обрані** ключі SharedState в NVS раз на хвилину.
НЕ зберігає все — тільки зареєстровані ключі.

Приклади: `persist.total_runtime`, `persist.compressor_cycles`,
`persist.boot_count`, `persist.last_defrost_time`.

```cpp
class PersistService : public BaseModule {
public:
    uint32_t save_interval_ms = 60000;  // раз на хвилину

    void track(const char* key);    // додати до автозбереження
    void untrack(const char* key);
    bool force_save();              // при shutdown
};
```

## 5. LoggerService — Логування

Ring buffer на 64 записи в RAM. Опціонально — flush на LittleFS.

```cpp
struct LogEntry {
    uint64_t timestamp_us;
    LogLevel level;           // DEBUG, INFO, WARNING, ERROR, CRITICAL
    etl::string<16> source;   // ім'я модуля
    etl::string<64> message;
};

class LoggerService : public BaseModule {
public:
    // Shortcut для модулів
    void log(LogLevel level, const char* source, const char* fmt, ...);

    // Читання (для WebSocket)
    using EntryCallback = void(*)(const LogEntry&, void*);
    void read_recent(size_t count, EntryCallback cb, void* user_data);

    // Налаштування
    LogLevel min_level = LogLevel::INFO;
    bool persist_to_flash = false;
    uint32_t flash_write_interval_ms = 10000;
};
```

## 6. SystemMonitor — Моніторинг ресурсів

Публікує стан системи в SharedState та на шину.

```cpp
class SystemMonitor : public BaseModule {
public:
    uint32_t report_interval_ms = 5000;

    // Порогові значення для аварій
    uint32_t heap_warning_threshold  = 20000;  // bytes
    uint32_t heap_critical_threshold = 10000;  // bytes

    bool on_init() override {
        // ─── Boot reason tracking ───
        esp_reset_reason_t reason = esp_reset_reason();
        state_set_int("system.reset_reason", (int32_t)reason);
        state_set_int("system.boot_count", load_boot_count_nvs());

        if (reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT) {
            // Останній перезапуск був через watchdog!
            // Повідомити ErrorService
        }
        return true;
    }

    void on_update(uint32_t dt_ms) override {
        // Оновлює: system.free_heap, system.min_free_heap,
        // system.uptime, system.cpu_temp
        // Перевіряє пороги heap → звертається до ErrorService
    }
};
```

**Boot reason** — критично для діагностики в полі.
Якщо ESP32 перезавантажився через watchdog — це ознака бага.
Якщо через brownout — проблема з живленням.
