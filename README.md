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
| inspect frozen architecture decisions | `docs/Librova-Architecture.md` |
| understand test expectations | `docs/engineering/TestStrategy.md` |
| change IPC / Protobuf transport safely | `docs/engineering/TransportInvariants.md` |
| change UI appearance or layout | `docs/UiDesignSystem.md` |
| inspect active work | `docs/backlog.yaml` or `python scripts\backlog.py list` |

## Recommended Read Order Before Substantive Work

1. `docs/Librova-Product.md`
2. `docs/CodebaseMap.md`
3. `AGENTS.md`
4. `docs/backlog.yaml`
5. Then the domain-specific reference you are about to touch (`TransportInvariants`, `TestStrategy`, `UiDesignSystem`, or `Librova-Architecture`)

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
