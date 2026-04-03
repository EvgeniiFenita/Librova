# Librova

Librova is a Windows-first desktop application for managing a personal e-book library.

The project uses a two-process architecture:

- `Librova.UI` in `C# / .NET / Avalonia`
- `Librova.Core` in `C++20`

Processes communicate through Protobuf over Windows named pipes. Local storage is based on SQLite with `FTS5`.

Current scope includes:

- importing `EPUB`, `FB2`, and `ZIP`;
- storing books in a managed library;
- metadata search and browsing;
- export;
- delete through the Windows `Recycle Bin`, with managed `Trash` fallback;
- offline-first single-user workflow.

Librova is not an e-book reader. It manages the library itself: import, storage, search, export, and maintenance.

Project documentation:

- `docs/Librova-Product.md`
- `docs/Librova-Architecture.md`
- `docs/Librova-Backlog.md`
