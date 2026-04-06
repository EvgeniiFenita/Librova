# Librova: Настройки и Диагностика — Ручные Сценарии

> Инструкция по запуску и полный реестр сценариев: [ManualUiTestScenarios.md](../ManualUiTestScenarios.md)

## 1. Open Or Create Library

1. В левой панели найди блок `Current Library`.
Ожидаемое поведение:
- под путем к библиотеке виден summary `Library: ...` с общим числом managed books и единым total library size в `MB`, включая `Covers` и `Database/librova.db`.
2. Нажми `Open Library...` и выбери другой существующий library root.
Ожидаемое поведение:
- текущая библиотека закрывается.
- приложение показывает loading state и затем открывает новую библиотеку.
- `Library` screen теперь работает уже с новым root.
2.1. Повтори `Open Library...`, но выбери обычную непустую папку без структуры Librova.
Ожидаемое поведение:
- переключение библиотеки не начинается;
- validation error объясняет, что `Open Library...` требует существующую библиотеку Librova.
3. Нажми `Create Library...` и выбери новый пустой путь.
Ожидаемое поведение:
- Librova создает managed library structure в новом месте.
- приложение переключается на новую библиотеку как на активную.
3.1. Повтори `Create Library...`, но выбери обычную непустую папку.
Ожидаемое поведение:
- переключение библиотеки не начинается;
- validation error объясняет, что `Create Library...` требует пустую целевую папку.
3.2. Повтори `Create Library...` с новым пустым путем, содержащим кириллицу.
Ожидаемое поведение:
- библиотека создается успешно;
- diagnostics и `host.log` продолжают показывать этот путь корректным Unicode-текстом.

## 2. Settings: Converter Configuration

1. Открой `Settings`.
2. Выбери `BuiltInFb2Cng`.
3. Введи относительный путь к executable.
Ожидаемое поведение:
- показывается converter validation error.
- сохранение недоступно.
4. Введи абсолютный путь к executable.
Ожидаемое поведение:
- validation error исчезает.
4.1. Нажми `Browse...` рядом с built-in executable path и выбери `fbc.exe`.
Ожидаемое поведение:
- путь к `fbc.exe` подставляется в поле автоматически;
- путь остается абсолютным;
- если был validation error из-за пустого пути, он исчезает после валидного выбора файла.
4.2. Проверь built-in `fb2cng` block целиком.
Ожидаемое поведение:
- в текущем UI нет отдельного поля для YAML-конфига `fbc.yaml`;
- для built-in режима на экране остаётся только путь к `fbc.exe`.
5. Нажми `Save`.
Ожидаемое поведение:
- converter settings успешно сохраняются.
- после `Save` текущая shell session перезагружается и начинает работать уже с обновленной converter-конфигурацией;
- после `Save` приложение остается на вкладке `Settings`, а не перебрасывает пользователя в `Library`;
- если после этого перейти в `Library` и выбрать `FB2`, кнопка `Export As EPUB` становится активной;
- если до настройки converter выбрать `FB2` в `Library`, вместо `Export As EPUB` показывается подсказка про `Settings`;
- если после этого перейти в `Import`, появляется чекбокс `Force conversion to EPUB during import`.
- кнопка `Save` оформлена как основное действие, а не как вторичная навигационная кнопка.
6. В built-in режиме через `fbc.exe` выполни импорт `FB2` с включенным `Force conversion to EPUB during import`, затем выполни `Export As EPUB` для другой `FB2` книги.
Ожидаемое поведение:
- рядом с import working directory и рядом с user-chosen export destination не появляются `fb2cng.log` или `fb2cng-report.zip`;
- если `fbc.exe` пишет свои стандартные log/report файлы, они оказываются внутри `Library\\Logs`.

## 3. Diagnostics And Logs

1. Открой `Settings`.
2. Посмотри блок `Diagnostics`.
Ожидаемое поведение:
- видны UI log, host log, UI state, preferences и host executable paths.
3. Выполни обычный import, затем открой оба log-файла.
Ожидаемое поведение:
- UI log содержит startup, command и error-relevant entries.
- host log содержит host/runtime-side operational entries.
- логи полезны для диагностики и не зашумлены частым polling.
- при уже открытой библиотеке оба runtime log-файла лежат в `LibraryRoot\Logs`.
4. Выполни batch, directory или `.zip` import, где есть хотя бы один skipped или failed source/entry.
Ожидаемое поведение:
- `host.log` содержит отдельные записи с конкретными именами проблемных файлов или ZIP entry, а не только общий итоговый summary;
- для skipped/failed ZIP entry в `host.log` видны имя архива, имя entry и причина;
- для failed или skipped source в batch / directory import в `host.log` видны путь источника и причина.

## 4. Settings: About Card

1. Перейди в секцию `Settings`.
Ожидаемое поведение:
- виден отдельный блок `ABOUT` с иконкой книги и заголовком `Librova`;
- блок выглядит визуально более выраженным, чем блок `Diagnostics` (более светлый фон карточки);
- в блоке показаны три строки: `VERSION`, `AUTHOR`, `CONTACT`;
- `VERSION` содержит текущую версию приложения (не пустое значение и без placeholder);
- `AUTHOR` содержит имя автора `Evgenii Volokhovich`;
- `CONTACT` содержит email `evgenii.github@gmail.com`.
