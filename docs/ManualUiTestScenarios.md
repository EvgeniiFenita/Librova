# Librova: Сценарии Ручного UI-Тестирования

Этот документ задает чеклист ручной проверки текущего MVP UI.

Запускать приложение через:

- `.\Run-Librova.ps1`

Перед началом:

- для обычного запуска держи открытыми:
  - [out\runtime\library\Logs\ui.log](C:\Users\evgen\Desktop\Librova\out\runtime\library\Logs\ui.log)
  - [out\runtime\library\Logs\host.log](C:\Users\evgen\Desktop\Librova\out\runtime\library\Logs\host.log)
- для `-FirstRun` и `-SecondRun` до выбора или открытия библиотеки UI временно пишет bootstrap log в:
  - [out\runtime\bootstrap-ui.log](C:\Users\evgen\Desktop\Librova\out\runtime\bootstrap-ui.log)
- после успешного выбора или открытия библиотеки bootstrap log должен быть перенесен в:
  - [out\runtime\library\Logs\ui.log](C:\Users\evgen\Desktop\Librova\out\runtime\library\Logs\ui.log)

Используй эти сценарии после заметных UI или runtime изменений и перед тем, как считать сборку стабильной.

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
- создается [out\runtime\ui-preferences.json](C:\Users\evgen\Desktop\Librova\out\runtime\ui-preferences.json).
- в выбранной папке появляется managed library structure: `Database`, `Books`, `Covers`, `Temp`, `Logs`, `Trash`.
- UI startup entries оказываются в `Logs\ui.log` внутри выбранной библиотеки.

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

## 4. Shell Navigation

1. В левой навигации нажми `Library`.
Ожидаемое поведение:
- заголовок в header становится `Library`.
- в основной области виден только library content.
2. Нажми `Import`.
Ожидаемое поведение:
- заголовок становится `Import`.
- виден только import content.
3. Нажми `Settings`.
Ожидаемое поведение:
- заголовок становится `Settings`.
- виден только settings content.
4. Прокрути каждый раздел.
Ожидаемое поведение:
- контент прокручивается вертикально.
- панели не накладываются друг на друга.

## 4.1 Host Lifetime

1. Запусти `.\Run-Librova.ps1` и дождись открытия основного окна.
2. Открой `Task Manager` и найди `Librova.UI.exe` и `LibrovaCoreHostApp.exe`.
3. Закрой UI обычным способом.
Ожидаемое поведение:
- `LibrovaCoreHostApp.exe` тоже завершается и не остается висеть фоновым процессом.
4. Повтори запуск приложения.
5. Принудительно заверши `Librova.UI.exe` через `Task Manager`.
Ожидаемое поведение:
- `LibrovaCoreHostApp.exe` тоже исчезает вскоре после смерти UI.
- host не остается orphaned process после crash-like termination UI.

## 5. Import Validation

1. Открой раздел `Import`.
2. Не выбирай файл и не делай drag-and-drop.
Ожидаемое поведение:
- большая зона импорта и кнопка `Select Files...` остаются доступны.
- import не стартует сам по себе без выбранного файла.
3. Нажми `Select Files...` и выбери реальный `.txt` файл.
Ожидаемое поведение:
- validation message сообщает, что поддерживаются только `.fb2`, `.epub`, `.zip`.
- import не запускается.
4. Нажми `Select Files...` и выбери валидный `.fb2`, `.epub` или `.zip`.
Ожидаемое поведение:
- import стартует без дополнительной кнопки `Start Import`.
- экран переходит в running state.
4.1. Нажми `Select Files...` и выбери одновременно один валидный `.fb2/.epub/.zip` и один неподдерживаемый файл, например `.txt`.
Ожидаемое поведение:
- batch import не блокируется целиком только из-за неподдерживаемого файла;
- валидные источники продолжают импортироваться;
- неподдерживаемые источники должны отражаться в итоговых warnings/summary, а не ломать весь запуск.
5. Нажми `Select Folder...` и выбери папку, где есть вложенные `.fb2`, `.epub` или `.zip`.
Ожидаемое поведение:
- import стартует для выбранной директории.
- supported книги и архивы ищутся рекурсивно.

## 6. Import Workflow

1. В `Import` нажми `Select Files...` и выбери валидный `.fb2`, `.epub` или `.zip`.
Ожидаемое поведение:
- status меняется с idle на running/completed.
- во время импорта главный сценарий экрана блокируется.
- активной остается только `Cancel`.
1.1. Повтори сценарий, выбрав сразу несколько файлов через `Select Files...`.
Ожидаемое поведение:
- import стартует как batch import;
- summary показывает агрегированный результат по всем выбранным источникам.
1.2. Повтори сценарий через `Select Folder...` для папки с вложенными книгами.
Ожидаемое поведение:
- import summary отражает рекурсивно найденные и обработанные файлы.
2. Пока import идет, попробуй переключиться на `Library` или `Settings`.
Ожидаемое поведение:
- навигация недоступна до завершения или отмены импорта.
3. Если job еще выполняется, нажми `Cancel`.
Ожидаемое поведение:
- status отражает cancellation request или cancelled result.
4. После завершения импорта посмотри блок результата.
Ожидаемое поведение:
- показываются summary, warnings и error state последнего импорта.
- browser позже может быть обновлен автоматически.

## 7. Drag And Drop Import Source

1. Открой раздел `Import`.
2. Перетащи на главное окно локальный `.fb2`, `.epub`, `.zip`, несколько файлов сразу или папку.
Ожидаемое поведение:
- приложение переключается на `Import`, если нужно.
- import стартует автоматически.
- не происходит crash или дублирования UI state.

## 8. Library Browser Basics

1. Открой `Library`.
Ожидаемое поведение:
- верхняя панель с `Search by title or author...`, фильтром языка и счетчиком книг остается сверху.
- карточки книг отображаются в адаптивной сетке.
2. Если книги есть, нажми на одну карточку книги.
Ожидаемое поведение:
- справа открывается `Selection Details`.
- сетка сжимается, а не перекрывается overlay.
 - если `Selection Details` закрыть, сетка снова занимает полную ширину секции `Library`.
3. Нажми на ту же карточку повторно.
Ожидаемое поведение:
- `Selection Details` закрывается.
6. Поводи мышью по карточкам и выбери несколько разных книг.
Ожидаемое поведение:
- hover не вызывает заметного скачка текста или перестройки карточки;
- выбор книги не вызывает массового мерцания всей сетки.
4. Если книг нет, посмотри на основную область.
Ожидаемое поведение:
- показывается пустое состояние `Library is empty` или `Nothing found`, а не пустой сломанный блок.
5. Используй `Search by title or author...` и фильтр языка.
Ожидаемое поведение:
- сетка обновляется без кнопки `Apply`.
- счетчик книг меняется вместе с текущей выдачей.

## 9. Full Details Loading

1. В `Library` выбери книгу.
Ожидаемое поведение:
- правая панель сама загружает детали выбранной книги.
- панель остается справа внутри секции `Library`, а не уезжает за экран или в отдельную прокручиваемую область.
- отображаются richer metadata, например publisher, year, language, format и genres, если они доступны.
- аннотация показывается в отдельном прокручиваемом блоке.

## 9.1 Covers And Legacy FB2 Metadata

1. Импортируй `FB2`, у которого есть извлеченная cover в `Covers`.
Ожидаемое поведение:
- в карточке книги отображается реальная cover, а не placeholder gradient.
2. Выбери эту книгу в `Library`.
Ожидаемое поведение:
- в `Selection Details` тоже отображается реальная cover.
3. Импортируй `FB2` с русскими метаданными и `encoding="windows-1251"`.
Ожидаемое поведение:
- импорт завершается без crash;
- книга появляется в `Library`;
- title и author отображаются корректным Unicode-текстом, без битой кодировки и без ошибки `String is invalid UTF-8`.

## 10. Export Book

1. В `Library` выбери книгу.
2. В правой панели нажми `Export`.
3. Выбери destination path вне managed library.
Ожидаемое поведение:
- export успешно завершается.
- destination file появляется на диске.
- исходный managed file не меняется.

## 11. Move To Trash

1. В `Library` выбери книгу.
2. В правой панели нажми `Move To Trash`.
Ожидаемое поведение:
- выбранная книга исчезает из browser после refresh.
- файл появляется внутри managed library в области `Trash`.
- если у книги была cover, cover переносится согласованно.

## 12. Import Updates Browser

1. Открой `Library` и запомни текущее количество книг.
2. Перейди в `Import` и импортируй новую валидную книгу.
3. Вернись в `Library`, если нужно.
Ожидаемое поведение:
- browser обновляется автоматически после успешного импорта.
- новая книга появляется без ручного перезапуска приложения.

## 13. Open Or Create Library

1. В левой панели найди блок `Current Library`.
2. Нажми `Open Library...` и выбери другой существующий library root.
Ожидаемое поведение:
- текущая библиотека закрывается.
- приложение показывает loading state и затем открывает новую библиотеку.
- `Library` screen теперь работает уже с новым root.
3. Нажми `Create Library...` и выбери новый пустой путь.
Ожидаемое поведение:
- Librova создает managed library structure в новом месте.
- приложение переключается на новую библиотеку как на активную.

## 14. Settings: Converter Configuration

1. Открой `Settings`.
2. Выбери `BuiltInFb2Cng`.
3. Введи относительный путь к executable.
Ожидаемое поведение:
- показывается converter validation error.
- сохранение недоступно.
4. Введи абсолютный путь к executable.
Ожидаемое поведение:
- validation error исчезает.
5. Переключись на `CustomCommand`.
6. Введи абсолютный путь к executable, но оставь arguments пустыми.
Ожидаемое поведение:
- validation error сообщает, что arguments обязательны.
7. Добавь по одной argument template на строку и нажми `Save`.
Ожидаемое поведение:
- converter settings успешно сохраняются.

## 15. Diagnostics And Logs

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

## 16. Relaunch State Persistence

1. В `Import` выбери валидный `Source File`.
2. Нормально закрой приложение.
3. Снова запусти `.\Run-Librova.ps1`.
Ожидаемое поведение:
- раздел `Import` восстанавливает предыдущий source path и последнее состояние import flow, если оно было сохранено.

## 17. Smoke Pass After Release Build

1. Выполни:
   `.\Run-Tests.ps1 -Preset x64-release -Configuration Release`
2. Выполни:
   `.\Run-Librova.ps1 -Preset x64-release -Configuration Release`
Ожидаемое поведение:
- release build завершается успешно.
- UI успешно запускается из release artifacts.
- host корректно резолвится из `Release` layout.
