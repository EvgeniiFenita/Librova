# Librova Product

## 1. What Librova Is

Librova is a Windows-first desktop application for managing a personal e-book library.

The product is intentionally narrower and simpler than Calibre. The MVP is meant to provide a reliable and understandable tool for:

- importing books into a managed library;
- storing metadata and covers;
- searching and browsing the library;
- exporting books;
- moving books to trash;
- working with `EPUB`, `FB2`, and `ZIP`.

Librova is not a reader. The application manages a library rather than reading books inside the UI.

## 2. Target User And Usage Model

The current MVP is built for:

- one user;
- one managed library;
- local, offline-first usage;
- a desktop workflow without cloud dependencies.

## 3. Core User Scenarios

### 3.1 First Launch

On first launch, the user chooses a library root. Under that root Librova stores:

- database;
- managed books;
- covers;
- logs;
- temporary import files;
- trash.

Once a library is active, both UI and native host logs are kept under that same library in `Logs/`, so diagnostics stay attached to the managed library rather than splitting across unrelated folders.

The native host is treated as a child runtime of the UI session. Normal shutdown and abnormal UI termination are both expected to stop the host process instead of leaving it orphaned in the background.

If the configured library root is invalid or unavailable on a later launch, Librova opens a startup recovery screen and lets the user choose a different library root before retrying startup.

If the library root itself is valid but the managed library is damaged, Librova does not silently recreate the library in place. Instead, it shows startup recovery guidance and requires the user to choose a different library root or repair the damaged library manually.

After startup, switching to another library is an application-level action rather than a hidden launch preference. The shell exposes `Open Library...` and `Create Library...` so the active library can be changed explicitly.

### 3.2 Book Import

The user can import:

- a single `EPUB`;
- a single `FB2`;
- multiple selected `EPUB` / `FB2` files in one batch;
- a `ZIP` containing `EPUB` and `FB2`.
- a directory, with recursive discovery of supported `FB2`, `EPUB`, and `ZIP` sources.

The import flow includes:

- file or folder selection from the UI;
- drag-and-drop of one or many files and folders;
- metadata parsing;
- duplicate detection;
- optional conversion `FB2 -> EPUB`;
- staging before commit;
- database write;
- managed storage placement.

The user-facing import screen is intentionally simplified: it centers on selecting or dropping a source file, then shows running progress and cancellation, rather than exposing internal job-management controls.

### 3.3 Library Browsing

The user can:

- search books by title or author through a single search field;
- filter the grid by language;
- browse books as cover-driven cards in a responsive grid;
- see stable-size book cards with a generated gradient placeholder when a cover is missing;
- open a right-hand details panel without leaving the `Library` section;
- inspect richer metadata for the selected book;
- export the selected book;
- move the selected book to trash.

Series and genres are part of the active MVP direction and are expected to become first-class browsing metadata rather than incidental parser output.

### 3.4 Export

The user can export a selected managed book to any destination path outside the managed library.

The export dialog should suggest a meaningful default filename derived from the book title and author, sanitized for Windows file-name rules.

### 3.5 Delete / Recycle

The current MVP keeps a safe user-facing delete action through the managed-library `Trash` flow.

Windows `Recycle Bin` integration is tracked as a future feature rather than an active MVP requirement.

### 3.6 Settings

The user can configure:

- the converter mode:
  - disabled
  - built-in `fb2cng`
  - custom external converter

## 4. What Is Included In The MVP

The current MVP includes:

- import of `EPUB`, `FB2`, and `ZIP`;
- duplicate handling;
- local managed storage;
- SQLite-based metadata storage and search;
- export;
- move to trash;
- a UI shell with sections `Library`, `Import`, and `Settings`;
- first-run setup;
- diagnostics and logging;
- a native host process behind the UI.

The active MVP backlog still includes:

- richer series and genres support;
- release-candidate stabilization and validation.

## 5. What Is Not Included In The MVP

The following are currently outside MVP scope:

- built-in book reading;
- metadata editing;
- cloud sync;
- multiple libraries;
- plugin ecosystem work;
- Windows `Recycle Bin` integration for delete;
- richer post-MVP library management features such as ratings, shelves, and favorites.

## 6. High-Level Technical Picture

Librova consists of two processes:

- `Librova.UI` in `C# / .NET / Avalonia`
- `Librova.Core` in `C++20`

They communicate through:

- `Protobuf`
- Windows named pipes

## 7. Current Project State

The MVP is now functionally close to completion.

Already implemented:

- import pipeline;
- browser and details with a card-grid `Library` view;
- export;
- delete-to-trash;
- first-run setup;
- converter settings and diagnostics;
- logging;
- a strong automated test baseline;
- a manual UI validation checklist.

Most remaining work now belongs to:

- series and genres support;
- stabilization and release-candidate hardening.

Planned future features include:

- Windows `Recycle Bin` integration.

## 8. Related Documents

- [Librova-Architecture](Librova-Architecture.md)
- [Librova-Roadmap](Librova-Roadmap.md)
- [ManualUiTestScenarios](ManualUiTestScenarios.md)
- [Development-History](archive/Development-History.md)
