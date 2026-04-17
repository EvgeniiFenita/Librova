# Librova: Удаление — Ручные Сценарии

> Инструкция по запуску и полный реестр сценариев: [ManualUiTestScenarios.md](../ManualUiTestScenarios.md)

## 1. Move To Recycle Bin

1. В `Library` выбери книгу.
2. В правой панели нажми `Move To Recycle Bin`.
Ожидаемое поведение:
- выбранная книга исчезает из browser после refresh.
- status text сообщает, что книга перемещена в `Recycle Bin`.
- book object исчезает из `Objects`, а cover object больше не остается активным артефактом библиотеки.
- кнопка `Move To Recycle Bin` визуально отделена от обычных secondary-кнопок как destructive-действие.
- если удаленная книга была последней книгой для какого-то языка, этот язык исчезает из language filter без ручного переключения в `All languages`.
3. Повтори `Move To Recycle Bin` после импорта через `Select Folder...`, выбрав книгу, попавшую в библиотеку из рекурсивно импортированной папки.
Ожидаемое поведение:
- удаление в `Recycle Bin` остается доступным;
- книга пропадает из browser, а остальные книги из того же directory import остаются доступными.
