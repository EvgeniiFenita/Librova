# Librova: Сценарии Ручного UI-Тестирования

Реестр всех сценариев ручной проверки UI. Сценарии разбиты по файлам в [`docs/manual-tests/`](manual-tests/).

Используй эти сценарии после заметных UI или runtime изменений и перед тем, как считать сборку стабильной.

## Как запускать

Запускать приложение через `.\Run-Librova.ps1`.

Перед началом держи открытыми:

- `LibraryRoot\Logs\ui.log`
- `LibraryRoot\Logs\host.log`

До выбора или открытия библиотеки UI пишет bootstrap log в runtime profile:

- обычный запуск: `%LOCALAPPDATA%\Librova\Logs\ui.log`
- portable package: `PortableData\Logs\ui.log`

После успешного выбора или открытия библиотеки UI пишет уже в `LibraryRoot\Logs\ui.log`.

## Реестр сценариев

Колонка **RG** (Release Gate) — сценарии, обязательные перед каждым релизом.

| Файл | # | Сценарий | RG |
|------|---|----------|----|
| [startup.md](manual-tests/startup.md) | 1 | First Run Setup And Portable Library Open | ✓ |
| [startup.md](manual-tests/startup.md) | 2 | Startup Error Recovery | ✓ |
| [startup.md](manual-tests/startup.md) | 3 | Startup Error Recovery: Corrupted Library | |
| [startup.md](manual-tests/startup.md) | 3.1 | Startup Error Recovery: Portable Saved Path Cannot Be Reopened | |
| [startup.md](manual-tests/startup.md) | 4 | Shell Navigation | ✓ |
| [startup.md](manual-tests/startup.md) | 4.1 | Host Lifetime | ✓ |
| [import.md](manual-tests/import.md) | 1 | Import Validation | ✓ |
| [import.md](manual-tests/import.md) | 1.3 | Import Validation: Async Source Validation Clears Before Start | |
| [import.md](manual-tests/import.md) | 1.4 | Import Validation: Empty Directory Produces Warning At Import Time | |
| [import.md](manual-tests/import.md) | 2 | Import Workflow | ✓ |
| [import.md](manual-tests/import.md) | 2.1 | Probable Duplicate Override | |
| [import.md](manual-tests/import.md) | 3 | Drag And Drop Import Source | ✓ |
| [import.md](manual-tests/import.md) | 4 | Import Updates Browser | ✓ |
| [library.md](manual-tests/library.md) | 1 | Library Browser Basics | ✓ |
| [library.md](manual-tests/library.md) | 2 | Full Details Loading | ✓ |
| [library.md](manual-tests/library.md) | 2.1 | Covers And Legacy FB2 Metadata | ✓ |
| [library.md](manual-tests/library.md) | 2.2 | Placeholder Covers Stay Fully Visible | |
| [library.md](manual-tests/library.md) | 3 | Library Sort Controls | ✓ |
| [export.md](manual-tests/export.md) | 1 | Export Book | ✓ |
| [export.md](manual-tests/export.md) | 2 | Export as EPUB Progress Feedback | ✓ |
| [delete.md](manual-tests/delete.md) | 1 | Move To Recycle Bin | ✓ |
| [settings.md](manual-tests/settings.md) | 1 | Open Or Create Library | ✓ |
| [settings.md](manual-tests/settings.md) | 1.1 | Open Or Create Library Rejects Nested Current-Library Folders | |
| [settings.md](manual-tests/settings.md) | 2 | Settings: Converter Configuration | ✓ |
| [settings.md](manual-tests/settings.md) | 3 | Diagnostics And Logs | |
| [settings.md](manual-tests/settings.md) | 4 | Settings: About Card | |
| [regression.md](manual-tests/regression.md) | 1 | Relaunch State Persistence | |
| [regression.md](manual-tests/regression.md) | 2 | Smoke Pass After Release Build | ✓ |
