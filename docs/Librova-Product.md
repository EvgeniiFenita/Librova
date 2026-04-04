# Librova Product

## 1. What Librova Is

Librova is a Windows-first desktop application for managing a personal e-book library.

The product is intentionally narrower and simpler than Calibre. Librova is meant to provide a reliable and understandable tool for:

- importing books into a managed library;
- storing metadata and covers;
- searching and browsing the library;
- exporting books;
- moving books to the Windows `Recycle Bin`, with managed-library `Trash` fallback when handoff fails;
- working with `EPUB`, `FB2`, and `ZIP`.

Librova is not a reader. The application manages a library rather than reading books inside the UI.

## 2. Target User And Usage Model

The current product direction is built for:

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

For the portable packaged distribution, bootstrap-only UI files that exist before any library is opened stay beside the application under `PortableData/`, including the pre-library UI log, saved UI preferences, and UI shell state. Once a library is active, the running session still moves to `LibraryRoot/Logs` for both UI and native host logs.

For multi-file, directory, and ZIP imports, the native host log must also record per-source and per-entry skipped/failed diagnostics with concrete file names, so large import investigations do not depend only on the UI warning summary.

The native host is treated as a child runtime of the UI session. Normal shutdown and abnormal UI termination are both expected to stop the host process instead of leaving it orphaned in the background.

If the configured library root is invalid or unavailable on a later launch, Librova opens a startup recovery screen and lets the user choose a different library root before retrying startup.

If the library root itself is valid but the managed library is damaged, Librova does not silently recreate the library in place. Instead, it shows startup recovery guidance and requires the user to choose a different library root or repair the damaged library manually.

After startup, switching to another library is an application-level action rather than a hidden launch preference. The shell exposes `Open Library...` and `Create Library...` so the active library can be changed explicitly.

`Open Library...` accepts only an existing Librova-managed library root that already contains the expected directory layout and `Database/librova.db`.

`Create Library...` accepts only a new or empty target directory and must not repurpose an unrelated non-empty folder as a managed library.

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
- explicit user opt-in when Librova allows a duplicate to be imported as a separate managed record;
- optional conversion `FB2 -> EPUB`, exposed as a checkbox in `Import` only when the current session has a configured converter;
- staging before commit;
- database write;
- managed storage placement.

The user-facing import screen is intentionally simplified: it centers on selecting or dropping a source file, then shows running progress and cancellation, rather than exposing internal job-management controls.

For multi-file imports, directory imports, and ZIP imports, the running state must surface a deterministic aggregate progress summary with total files, processed files, and the current imported / failed / skipped counts. The terminal result must keep the same aggregate summary visible after the job finishes.

When a batch import contains both valid sources and a broken ZIP archive, Librova should surface the archive failure as a per-source warning and continue importing the remaining valid sources instead of failing the entire batch during workload preparation.

If the user cancels a multi-file, directory, or ZIP import after some books were already imported, Librova must roll back those newly imported books so the managed library and filesystem return to the same state they had before the cancelled import started.

### 3.3 Library Browsing

The user can:

- search books through a single full-text field that matches title, authors, tags, and description;
- use that same full-text field with ordinary punctuation without breaking the search flow through raw FTS syntax errors;
- filter the grid by language;
- see a library summary in the left `Current Library` panel with total managed-book count and total managed-book size in megabytes, excluding the database;
- browse books as cover-driven cards in a responsive grid;
- see stable-size book cards with a generated gradient placeholder when a cover is missing, while real covers keep their aspect ratio and sit on a neutral matte background instead of exposed color bands;
- open a right-hand details panel without leaving the `Library` section;
- keep the selected card visible by re-centering it in the scroll viewport when the details panel opens or closes and the grid reflows;
- keep desktop-oriented minimum window dimensions so the `Library` section still preserves multiple visible cards, at least two columns with the details panel open, and at least two visible rows of cards;
- inspect richer metadata for the selected book, including series and genres when the source file provides them;
- export the selected book;
- move the selected book to the Windows `Recycle Bin`.

Series and genres are already surfaced in the details panel when available; first-class browser filtering and browsing workflows for them remain future work.

### 3.4 Export

The user can export a selected managed book to any destination path outside the managed library.

The export dialog should suggest a meaningful default filename derived from the book title and author, sanitized for Windows file-name rules.

If the active session has a configured converter, managed `FB2` books can also be exported as `EPUB` through a dedicated `Export As EPUB` action.

If a selected managed book is `FB2` but the current session has no configured converter, the details panel does not show the `Export As EPUB` action and instead points the user to `Settings` to configure a converter.

### 3.5 Delete / Recycle

The primary user-facing delete action now sends managed books to the Windows `Recycle Bin`.

Delete still stages files through the managed-library `Trash` area first so rollback remains explicit until the database row is removed.

If the final handoff into the Windows `Recycle Bin` fails, Librova keeps the deleted files in the managed-library `Trash` area and reports that fallback explicitly in the UI status text.

### 3.6 Settings

The user can configure:

- the converter mode:
  - disabled
  - built-in `fb2cng`
  - custom external converter
- the built-in `fb2cng` executable path through direct text entry or a `Browse...` file picker in `Settings`
- the custom converter executable path through direct text entry or a `Browse...` file picker in `Settings`

Saving converter settings reloads the current shell session so the active host process immediately picks up the new converter configuration.

That reload keeps the user in the current section instead of forcing a jump back to `Library`.

For the built-in `fb2cng` / `fbc.exe` profile, Librova runs the converter with the managed library `Logs` directory as its process working directory so default `fb2cng.log` output and similar diagnostic artifacts do not pollute import temp folders or user export destinations.

The `Settings` UI does not expose a separate YAML config path for built-in `fb2cng`; the shell only asks for the executable path.

## 4. Current Included Scope

The current implemented scope includes:

- import of `EPUB`, `FB2`, and `ZIP`;
- duplicate handling;
- local managed storage;
- SQLite-based metadata storage and search;
- export;
- move to the Windows `Recycle Bin`, with managed-library `Trash` fallback if needed;
- a UI shell with sections `Library`, `Import`, and `Settings`;
- first-run setup;
- diagnostics and logging;
- a native host process behind the UI.

The active open work is tracked only in the single project backlog document.

## 5. Currently Not Included

The following are currently outside the implemented scope:

- built-in book reading;
- metadata editing;
- cloud sync;
- multiple libraries;
- plugin ecosystem work;
- richer library management features such as ratings, shelves, and favorites.

## 6. High-Level Technical Picture

Librova consists of two processes:

- `Librova.UI` in `C# / .NET / Avalonia`
- `Librova.Core` in `C++20`

They communicate through:

- `Protobuf`
- Windows named pipes

## 7. Current Project State

The current product baseline is now functionally close to completion.

Already implemented:

- import pipeline;
- browser and details with a card-grid `Library` view;
- export;
- delete through the Windows `Recycle Bin`, with managed-library `Trash` fallback;
- first-run setup;
- converter settings and diagnostics;
- logging;
- a strong automated test baseline;
- a manual UI validation checklist.

Any remaining implementation work belongs in [Librova Backlog](Librova-Backlog.md).

## 8. Related Documents

- [Librova-Architecture](Librova-Architecture.md)
- [Librova Backlog](Librova-Backlog.md)
- [ManualUiTestScenarios](ManualUiTestScenarios.md)
