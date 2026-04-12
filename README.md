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

## Branch structure

| Branch | Purpose |
|---|---|
| `main` | Stable releases only — each commit corresponds to a release tag |
| `dev/<version>` | Development branch for the next release (WIP — not yet released) |

Development is never done directly on `main`. See `AGENTS.md § Branching Policy` for the full workflow.
