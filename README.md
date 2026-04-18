# Librova

Librova is a Windows-first desktop application for managing a personal e-book library.

The project uses a two-process architecture:

- `Librova.UI` in `C# / .NET / Avalonia`
- `Librova.Core` in `C++20`

Processes communicate through Protobuf over Windows named pipes. Local storage is based on SQLite with `FTS5`.

Librova is not an e-book reader. It manages the library itself: import, storage, search, export, and maintenance.

## Current Product Scope

- import `EPUB`, `FB2`, and `ZIP`
- store books in a managed library
- search and browse metadata
- export managed books
- delete through the Windows `Recycle Bin`, with managed `Trash` fallback
- run as an offline-first single-user desktop workflow

## Start With The Right Document

| If you need to... | Read this |
|---|---|
| understand what Librova is supposed to do | `docs/Librova-Product.md` |
| orient in the codebase and find the right module fast | `docs/CodebaseMap.md` |
| follow repository-wide rules, workflow, and doc ownership | `AGENTS.md` |
| inspect frozen architecture decisions | `docs/CodebaseMap.md` §14 Architecture Decisions |
| understand test expectations | `AGENTS.md` § Verification and test discipline |
| change IPC / Protobuf transport safely | `docs/CodebaseMap.md` §5 IPC Boundary |
| change UI appearance or layout | `docs/UiDesignSystem.md` |
| inspect active work | `python scripts\backlog.py list` or `python scripts\backlog.py show <id>` |

## Common Commands

```powershell
.\Run-Tests.ps1
python scripts\backlog.py list
scripts\ValidateProto.ps1
.\Run-Librova.ps1
```

For agent-oriented task procedures, see the skills listed in `AGENTS.md`.

## Branch Structure

| Branch | Purpose |
|---|---|
| `main` | Stable releases only — each commit corresponds to a release tag |
| `dev/<version>` | Development branch for the next release (WIP — not yet released) |

Development is never done directly on `main`. See `AGENTS.md § Branching Policy` for the full workflow.
