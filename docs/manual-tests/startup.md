# Librova: Запуск и Навигация — Ручные Сценарии

> Инструкция по запуску и полный реестр сценариев: [ManualUiTestScenarios.md](../ManualUiTestScenarios.md)

## 1. First Run Setup

1. Удали `out\runtime\ui-preferences.json` и `out\runtime\ui-shell-state.json`.
2. Запусти `.\Run-Librova.ps1 -FirstRun`.
3. Убедись, что в стартовом окне показан экран `Set up your Librova library`.
4. Нажми `Continue`, указав невалидный относительный путь.
Ожидаемое поведение:
- `Continue` неактивна или показывается validation error.
- host не стартует.
5. Нажми `Browse...` и выбери валидную пустую папку.
6. Нажми `Continue`.
Ожидаемое поведение:
- startup screen переходит в loading state, затем открывается основной shell приложения.
- создается [out\runtime\ui-preferences.json](out/runtime/ui-preferences.json).
- в выбранной папке появляется managed library structure: `Database`, `Books`, `Covers`, `Temp`, `Logs`, `Trash`.
- UI startup entries оказываются в `Logs\ui.log` внутри выбранной библиотеки.
7. Повтори `-FirstRun`, но выбери уже непустую папку, которая не является существующей библиотекой Librova.
Ожидаемое поведение:
- `Continue` не запускает host.
- validation error явно требует пустую папку для создания новой библиотеки.
8. Повтори `-FirstRun`, но выбери пустую папку с кириллическим путем, например `C:\Books\Моя библиотека`.
Ожидаемое поведение:
- setup и дальнейший startup завершаются успешно;
- в `Logs\host.log` и `Logs\ui.log` путь к библиотеке отображается без `????` и без mojibake.

## 2. Startup Error Recovery

1. Закрой приложение.
2. Отредактируй `out\runtime\ui-preferences.json`, чтобы `PreferredLibraryRoot` указывал на плохой или недоступный путь.
3. Запусти `.\Run-Librova.ps1 -SecondRun`.
Ожидаемое поведение:
- показывается startup error screen.
- на экране видны пути к UI log, UI state и preferences.
- на экране также есть блок `Choose Another Library Root`.
- пока библиотека еще не открыта, UI log path может указывать на bootstrap log в `out\runtime\bootstrap-ui.log`.
4. Введи валидную папку и нажми `Retry With This Library`.
Ожидаемое поведение:
- приложение успешно стартует без ручной чистки файлов.
- исправленный library root сохраняется для следующего запуска.

## 3. Startup Error Recovery: Corrupted Library

1. Закрой приложение.
2. Оставь валидный `PreferredLibraryRoot`, но повреди библиотеку внутри него, например испорть файл базы данных.
3. Запусти `.\Run-Librova.ps1 -SecondRun`.
Ожидаемое поведение:
- показывается `Startup Error Recovery`.
- startup error объясняет проблему открытия библиотеки заметно лучше, чем просто голый timeout.
- в поле `Choose Another Library Root` показан текущий library root.
- `Retry With This Library` неактивна, пока выбран тот же самый путь.
4. Введи другой валидный library root.
Ожидаемое поведение:
- `Retry With This Library` становится активной.
- приложение не пытается молча пересоздавать или чинить поврежденную библиотеку по тому же пути.
5. Введи путь к существующей, но не-Librova папке, и нажми `Retry With This Library`.
Ожидаемое поведение:
- recovery validation отклоняет путь как несуществующую managed library Librova;
- приложение не подменяет `Open Library` логикой неявного `Create Library`.

## 4. Shell Navigation

1. В левой навигации нажми `Library`.
Ожидаемое поведение:
- в верхней части левого rail под `Librova` показан нетехнический продуктовый слоган `Your shelf, your story`;
- отдельная верхняя hero-card с названием секции не показывается.
- в основной области виден только library content.
2. Нажми `Import`.
Ожидаемое поведение:
- отдельная верхняя hero-card с названием секции не показывается.
- виден только import content.
3. Нажми `Settings`.
Ожидаемое поведение:
- кнопка `Settings` находится внизу левой навигационной панели, отдельно от верхней рабочей навигации;
- отдельная верхняя hero-card с названием секции не показывается.
- виден только settings content.
4. Прокрути каждый раздел.
Ожидаемое поведение:
- контент прокручивается вертикально.
- панели не накладываются друг на друга.
- в заголовке окна и на панели задач у приложения есть собственная иконка, а не дефолтный системный placeholder.

## 4.1 Host Lifetime

1. Запусти `.\Run-Librova.ps1` и дождись открытия основного окна.
2. Открой `Task Manager` и найди `Librova.UI.exe` и `LibrovaCoreHostApp.exe`.
3. Закрой UI обычным способом.
Ожидаемое поведение:
- `LibrovaCoreHostApp.exe` тоже завершается и не остается висеть фоновым процессом.
- в `host.log` виден нормальный shutdown-path, а не только crash-like termination.
4. Повтори запуск приложения.
5. Принудительно заверши `Librova.UI.exe` через `Task Manager`.
Ожидаемое поведение:
- `LibrovaCoreHostApp.exe` тоже исчезает вскоре после смерти UI.
- host не остается orphaned process после crash-like termination UI.
