# Librova Product

## 1. Что Такое Librova

Librova - это Windows-first desktop application для управления личной библиотекой электронных книг.

Продукт намеренно уже и проще, чем Calibre. Цель MVP - дать надежный, понятный и технически устойчивый инструмент для:

- импорта книг в managed library;
- хранения metadata и обложек;
- поиска и просмотра библиотеки;
- экспорта книг;
- удаления книг в trash;
- работы с `EPUB`, `FB2` и `ZIP`.

Librova не является ридером. Приложение управляет библиотекой, а не чтением книг внутри UI.

## 2. Для Кого Этот Продукт

Текущий MVP рассчитан на:

- одного пользователя;
- одну managed library;
- локальное, offline-first использование;
- desktop workflow без облачных зависимостей.

## 3. Основные Пользовательские Сценарии

### 3.1 Первый Запуск

При первом запуске пользователь выбирает library root. Под этим корнем Librova хранит:

- database;
- managed books;
- covers;
- logs;
- temporary import files;
- trash.

### 3.2 Импорт Книг

Пользователь может импортировать:

- один `EPUB`;
- один `FB2`;
- `ZIP`, содержащий `EPUB` и `FB2`.

Импорт включает:

- parsing metadata;
- duplicate detection;
- optional conversion `FB2 -> EPUB`;
- staging before commit;
- запись в database;
- managed storage layout.

### 3.3 Просмотр Библиотеки

Пользователь может:

- искать книги;
- фильтровать по author, language, format;
- сортировать список;
- переключать страницы;
- смотреть details выбранной книги;
- запрашивать full details для выбранной записи.

### 3.4 Экспорт

Пользователь может экспортировать выбранную managed book в произвольный destination path вне managed library.

### 3.5 Удаление В Trash

Пользователь может переместить выбранную книгу в managed-library `Trash`, сохранив rollback-safe semantics на backend стороне.

### 3.6 Настройки

Пользователь может задать:

- library root для следующего запуска;
- converter mode для следующего запуска:
  - disabled
  - built-in `fb2cng`
  - custom external converter

## 4. Что Входит В MVP

В текущий MVP входят:

- import `EPUB`, `FB2`, `ZIP`;
- duplicate handling;
- local managed storage;
- SQLite-based metadata storage and search;
- export;
- move to trash;
- UI shell with sections `Library`, `Import`, `Settings`;
- first-run setup;
- diagnostics and logging;
- native host process behind the UI.

## 5. Что Не Входит В MVP

На текущем этапе не входят:

- built-in book reader;
- metadata editing;
- cloud sync;
- multiple libraries;
- plugin ecosystem;
- rich post-MVP library management features like ratings, shelves, favorites.

## 6. High-Level Техническая Картина

Librova состоит из двух процессов:

- `Librova.UI` на `C# / .NET / Avalonia`
- `Librova.Core` на `C++20`

Они общаются через:

- `Protobuf`
- Windows named pipes

## 7. Текущее Состояние Проекта

Сейчас MVP уже функционально близок к завершению.

Реально реализованы:

- import pipeline;
- browser/details;
- export;
- delete-to-trash;
- first-run setup;
- settings for next launch;
- logging;
- strong automated test baseline;
- manual UI validation checklist.

Оставшаяся работа уже в основном относится к:

- stabilization;
- final review;
- release-candidate hardening.

## 8. Связанные Документы

- [Librova-Architecture](C:\Users\evgen\Desktop\Librova\docs\Librova-Architecture.md)
- [Librova-Roadmap](C:\Users\evgen\Desktop\Librova\docs\Librova-Roadmap.md)
- [ManualUiTestScenarios](C:\Users\evgen\Desktop\Librova\docs\ManualUiTestScenarios.md)
- [Development-History](C:\Users\evgen\Desktop\Librova\docs\archive\Development-History.md)
