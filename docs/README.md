# ModESP v4 — Документація

## Архітектурна документація

| Файл | Що описує |
|------|-----------|
| [01_architecture.md](01_architecture.md) | Шари системи, залежності, потік даних |
| [02_core.md](02_core.md) | SharedState, BaseModule, ModuleManager, types.h |
| [03_services.md](03_services.md) | Error, Watchdog, Config, Persist, Logger, SystemMonitor |
| [04_best_practices.md](04_best_practices.md) | Правила коду: пам'ять, потоки, помилки, іменування |
| [05_cooling_defrost.md](05_cooling_defrost.md) | Thermostat + Defrost: state machines, фази, параметри |
| [06_roadmap.md](06_roadmap.md) | Дорожня карта: завершені та заплановані фази |
| [07_equipment.md](07_equipment.md) | Equipment Manager + Protection module |
| [08_webui.md](08_webui.md) | Svelte 4 WebUI: stores, widgets, pages, theme, i18n |
| [09_datalogger.md](09_datalogger.md) | DataLogger: 6-channel logging, ChartWidget, API |
| [10_manifest_standard.md](10_manifest_standard.md) | Стандарт маніфестів: Board, Driver, Module, Bindings |

## Інше

| Файл | Що описує |
|------|-----------|
| [CHANGELOG.md](CHANGELOG.md) | Повний changelog проекту (з 2026-02-16) |
| [archive/](archive/) | Історичні промпти, ревью, audit snapshots |

## Робочі документи (корінь проекту)

| Файл | Призначення |
|------|-------------|
| `CLAUDE.md` | Як працює проект ЗАРАЗ — читається Claude Code автоматично |
| `ACTION_PLAN.md` | Чеклісти задач, баги, TODO |
| `README.md` | Огляд проекту для GitHub |

## Changelog
- 2026-02-24 — Переписано як індекс. Додано docs 07-09, CHANGELOG, archive
- 2026-02-20 — Оновлено: додано ревью/аудит файли
- 2026-02-17 — Створено
