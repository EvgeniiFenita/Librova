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

If the configured library root is invalid or unavailable on a later launch, Librova opens a startup recovery screen and lets the user choose a different library root before retrying startup.

If the library root itself is valid but the managed library is damaged, Librova does not silently recreate the library in place. Instead, it shows startup recovery guidance and requires the user to choose a different library root or repair the damaged library manually.

### 3.2 Book Import

The user can import:

- a single `EPUB`;
- a single `FB2`;
- a `ZIP` containing `EPUB` and `FB2`.

The import flow includes:

- metadata parsing;
- duplicate detection;
- optional conversion `FB2 -> EPUB`;
- staging before commit;
- database write;
- managed storage placement.

### 3.3 Library Browsing

The user can:

- search books;
- filter by author, language, and format;
- sort the list;
- change pages;
- inspect details for the selected book;
- request fuller details for the selected entry.

### 3.4 Export

The user can export a selected managed book to any destination path outside the managed library.

### 3.5 Move To Trash

The user can move a selected book into the managed-library `Trash`, with rollback-safe backend semantics.

### 3.6 Settings

The user can configure:

- the library root for the next launch;
- the converter mode for the next launch:
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

## 5. What Is Not Included In The MVP

The following are currently outside MVP scope:

- built-in book reading;
- metadata editing;
- cloud sync;
- multiple libraries;
- plugin ecosystem work;
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
- browser and details;
- export;
- delete-to-trash;
- first-run setup;
- settings for the next launch;
- logging;
- a strong automated test baseline;
- a manual UI validation checklist.

Most remaining work now belongs to:

- stabilization;
- final review;
- release-candidate hardening.

## 8. Related Documents

- [Librova-Architecture](C:\Users\evgen\Desktop\Librova\docs\Librova-Architecture.md)
- [Librova-Roadmap](C:\Users\evgen\Desktop\Librova\docs\Librova-Roadmap.md)
- [ManualUiTestScenarios](C:\Users\evgen\Desktop\Librova\docs\ManualUiTestScenarios.md)
- [Development-History](C:\Users\evgen\Desktop\Librova\docs\archive\Development-History.md)
