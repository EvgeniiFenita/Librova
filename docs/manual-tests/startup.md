# Librova: Запуск и Навигация — Ручные Сценарии

> Инструкция по запуску и полный реестр сценариев: [ManualUiTestScenarios.md](../ManualUiTestScenarios.md)

## 1. First Run Setup And Portable Library Open

1. Для обычного запуска удали `%LOCALAPPDATA%\Librova\ui-preferences.json` и `%LOCALAPPDATA%\Librova\ui-shell-state.json`.
2. Для portable package удали `PortableData\ui-preferences.json` и `PortableData\ui-shell-state.json`.
3. Запусти `.\Run-Librova.ps1 -FirstRun`.
4. Убедись, что в стартовом окне показан экран `Set up your library`, а в setup-карточке есть выбор `Create New` и `Open Existing`.
5. Оставь выбранным `Create New`, нажми `Continue`, указав невалидный относительный путь.
Ожидаемое поведение:
- `Continue` неактивна или показывается validation error.
- host не стартует.
6. Оставь выбранным `Create New`, нажми `Browse...` и выбери валидную пустую папку.
7. Нажми `Continue`.
Ожидаемое поведение:
- startup screen переходит в loading state, затем открывается основной shell приложения.
- для обычного запуска создается `%LOCALAPPDATA%\Librova\ui-preferences.json`.
- для portable package создается `PortableData\ui-preferences.json`.
- в выбранной папке появляется managed library structure: `Database`, `Objects`, `Logs`, `Trash`.
- в библиотеке не создается runtime `Temp`; временные import/converter/runtime path живут в local runtime workspace или в `PortableData\Runtime`.
- после штатного закрытия или следующего успешного старта retained UI log оказывается в `Logs\ui.log` внутри выбранной библиотеки.
8. Повтори `-FirstRun`, но в setup переключись на `Open Existing` и выбери уже созданную существующую библиотеку Librova.
Ожидаемое поведение:
- кнопка действия меняет текст на `Open Library`;
- existing managed library проходит validation;
- после нажатия `Open Library` приложение открывает существующую библиотеку без пересоздания структуры.
9. Повтори `-FirstRun`, но оставь `Create New` и выбери уже непустую папку, которая не является существующей библиотекой Librova.
Ожидаемое поведение:
- `Continue` не запускает host.
- validation error явно требует пустую папку для создания новой библиотеки.
10. Повтори `-FirstRun`, оставь `Open Existing` и выбери пустую или обычную не-Librova папку.
Ожидаемое поведение:
- `Open Library` не запускает host;
- validation error явно требует существующую managed library Librova.
11. Повтори `-FirstRun`, но выбери пустую папку с кириллическим путем, например `C:\Books\Моя библиотека`.
Ожидаемое поведение:
- setup и дальнейший startup завершаются успешно;
- в `Logs\host.log` и `Logs\ui.log` путь к библиотеке отображается без `????` и без mojibake.
12. Для portable smoke: положи приложение и уже созданную managed library в один переносимый каталог, открой библиотеку, затем закрой приложение и перенеси весь каталог на другой путь или диск так, чтобы абсолютный путь к библиотеке изменился.
13. Снова запусти приложение из нового пути.
Ожидаемое поведение:
- portable launch не требует ручной правки `ui-preferences.json`;
- Librova автоматически открывает ту же библиотеку по новому пути;
- startup не падает в recovery screen только из-за смены drive letter или корневого каталога portable-пакета.

## 2. Startup Error Recovery

1. Закрой приложение.
2. Для обычного запуска отредактируй `%LOCALAPPDATA%\Librova\ui-preferences.json`, чтобы `PreferredLibraryRoot` указывал на плохой или недоступный путь.
3. Для portable package отредактируй `PortableData\ui-preferences.json` таким же образом.
4. Запусти `.\Run-Librova.ps1 -SecondRun`.
Ожидаемое поведение:
- показывается startup error screen.
- на экране видны пути к UI log, UI state и preferences.
- на экране также есть блок `Choose Another Library Root`.
- пока библиотека еще не открыта, UI log path может указывать на bootstrap log в `%LOCALAPPDATA%\Librova\Logs\ui.log` для обычного запуска или в `PortableData\Logs\ui.log` для portable package.
5. Введи валидную папку и нажми `Retry With This Library`.
Ожидаемое поведение:
- приложение успешно стартует без ручной чистки файлов.
- после следующего успешного старта retained library log снова находится в `LibraryRoot\Logs`.
- исправленный library root сохраняется для следующего запуска.

## 3. Startup Error Recovery: Corrupted Library

1. Закрой приложение.
2. Оставь валидный `PreferredLibraryRoot`, но повреди библиотеку внутри него, например испорть файл базы данных.
3. Запусти `.\Run-Librova.ps1 -SecondRun`.
Ожидаемое поведение:
- показывается startup error screen с заголовком `Startup failed`.
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
- приложение не подменяет `Open Library` логикой неявного `Continue`.

## 3.1 Startup Error Recovery: Portable Saved Path Cannot Be Reopened

1. Собери portable package и один раз открой через него библиотеку так, чтобы `PortableData\ui-preferences.json` сохранил путь к библиотеке.
2. Закрой приложение.
3. Сделай сохраненный путь недоступным: отключи флешку, переименуй каталог библиотеки или оставь в `ui-preferences.json` старый абсолютный путь, который больше не существует.
4. Снова запусти portable приложение.
Ожидаемое поведение:
- показывается startup error screen с заголовком `Startup failed`, а не экран первого запуска `Set up your library`;
- recovery screen объясняет, что сохраненную библиотеку не удалось открыть;
- в блоке выбора пути показан сохраненный library root или его последний известный hint;
- приложение не пытается молча открыть fallback library и не сбрасывает пользователя в режим создания новой библиотеки.

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
