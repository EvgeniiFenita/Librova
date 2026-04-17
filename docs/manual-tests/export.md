# Librova: Экспорт — Ручные Сценарии

> Инструкция по запуску и полный реестр сценариев: [ManualUiTestScenarios.md](../ManualUiTestScenarios.md)

## 1. Export Book

1. В `Library` выбери книгу.
2. В правой панели нажми `Export`.
3. Выбери destination path вне managed library.
Ожидаемое поведение:
- диалог предлагает имя файла, собранное из title и author, а недопустимые для Windows символы не попадают в suggested file name.
- export успешно завершается.
- destination file появляется на диске.
- исходный managed file не меняется.
- кнопка `Export` остается обычным secondary action в панели `Book Details`.
4. Если выбранная книга хранится как `FB2`, а приложение запущено с настроенным converter path, нажми `Export as EPUB`.
Ожидаемое поведение:
- в панели `Book Details` видна отдельная кнопка `Export as EPUB`;
- если converter path не настроен в текущей сессии, кнопка `Export as EPUB` для `FB2` не показывается, а вместо нее видна подсказка про настройку converter в `Settings`;
- dialog предлагает имя файла с расширением `.epub`;
- экспорт завершается созданием `.epub` файла вне managed library.
4.1. Повтори `Export` и `Export as EPUB` для `FB2`, который был импортирован без `Force conversion to EPUB during import`.
Ожидаемое поведение:
- обычный `Export` создает читаемый `.fb2` файл с тем же содержимым, что и исходная книга, несмотря на внутреннее сжатие managed файла;
- `Export as EPUB` тоже завершается успешно и не оставляет временные распакованные `FB2` в managed library; временное декодирование использует local runtime workspace, а не `Library\\Temp`.
5. Повтори `Export` после импорта через `Select Folder...`, выбрав книгу, попавшую в библиотеку из рекурсивно импортированной папки.
Ожидаемое поведение:
- экспорт остается доступен и успешно завершает копирование managed file;
- directory import не оставляет книгу в состоянии, где `Export` ломается или молча ничего не делает.

## 2. Export as EPUB Progress Feedback

1. Импортируй FB2-книгу (желательно сжатую или конвертируемую через fb2c).
2. Открой Library, выбери FB2-книгу.
3. Убедись, что конвертер настроен в Settings.
4. Нажми Export as EPUB в панели деталей. В диалоге выбери целевой файл.
Ожидаемое поведение:
- Сразу после закрытия диалога сохранения в нижней части панели деталей появляется блок с indeterminate progress bar (amber/gold).
- Над progress bar отображается текст: Exporting '<Title>' as EPUB....
- Кнопки Export as EPUB, Export и Move to Recycle Bin заблокированы на всё время экспорта.
- По завершении progress bar исчезает, в статус-тексте появляется Exported '<Title>' as EPUB to '<path>'.
- Если нажать Export (не EPUB), аналогичный progress блок тоже отображается (Exporting '<Title>'...).
