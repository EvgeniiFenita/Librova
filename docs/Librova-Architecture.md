# Librova Architecture

## 1. Runtime Model

Librova is a two-process desktop application:

- `Librova.UI` in `C# / .NET / Avalonia`
- `Librova.Core` in `C++20`

The UI is responsible for:

- windowing;
- user interactions;
- desktop dialogs;
- ViewModels and UI-facing services.

The native core is responsible for:

- domain logic;
- import pipeline;
- parsing;
- conversion orchestration;
- persistence;
- managed storage;
- jobs;
- search;
- file export and staged delete behavior that targets the Windows `Recycle Bin`.

Any remaining implementation work on top of that baseline is tracked in the project backlog.

## 2. IPC Boundary

The transport is:

- `Protobuf` contracts
- over Windows named pipes

This is the canonical runtime boundary between UI and core.

For process lifetime safety on Windows, the UI also passes its parent process id to the native host and binds the host lifetime to the UI session, so the host is not expected to survive a crashed or abruptly terminated UI process.

For normal UI shutdown, the UI must first request a graceful host stop and only fall back to forced termination if that request times out.

The runtime deliberately does not depend on:

- `gRPC` runtime
- `P/Invoke` as the main architecture

## 3. Storage And Persistence

### 3.1 Database

- SQLite is the local database.
- Search uses relational filters plus `FTS5` text search.
- Unicode correctness is required across storage and search boundaries.

#### 3.1.1 Write-Session Lifecycle

`CSqliteBookRepository` keeps a single SQLite write connection open for the duration of the repository's lifetime rather than opening and closing one per operation. This connection is held in a mutex-protected write session that serialises concurrent write calls.

In production, the connection is held until the host process terminates and is never explicitly closed. In tests that create a `CSqliteBookRepository` over a temp file, `repository.CloseSession()` must be called before the database file is removed, because Windows does not allow deleting a file that has an open file handle.

### 3.2 Managed Library

One managed library root contains:

- `Database`
- `Books`
- `Covers`
- `Logs`
- `Temp`
- `Trash`

Managed paths are stable and `BookId`-based.

Managed file bytes may use an internal storage encoding that is independent from the logical book format. In the current implementation, fallback-managed `FB2` files are stored compressed inside `Books/`, while browse, export, delete, and duplicate behavior continue to treat them as ordinary `FB2` books.

The `Trash` directory remains part of the implemented baseline as rollback-safe staging for delete operations.

The user-facing delete path now removes managed books from the catalog, stages their files under `Trash`, and then hands those staged files off to the Windows `Recycle Bin`.

If the Windows handoff fails, the delete remains committed in the catalog and the staged files stay in `Trash` as an explicit managed fallback.

Library-root bootstrap is mode-specific:

- `Create Library` may initialize only a new or empty target directory.
- `Open Library` must validate that the selected root is already a complete managed library, including `Database/librova.db`.
- startup recovery for a damaged library must not silently recreate that library in place.

### 3.3 Import Safety

Imports are staged before commit.

The system is expected to:

- avoid partial visible success where possible;
- clean stale temp state on startup;
- keep rollback/failure semantics explicit.

The import pipeline now accepts one or many selected source paths, including folders. Directory import follows the same transactional and summary-oriented principles as single-file import while scaling to recursive scans over mixed-content folders.

Duplicate detection operates at two layers:

- **Read side** (`FindDuplicates`): checked before any staging begins; returns early with `RejectedDuplicate` or `DecisionRequired` on matches.
- **Write side** (`IBookRepository::Add`): re-checks the `sha256_hex` hash inside a `BEGIN IMMEDIATE` SQLite transaction at insert time, closing the race window between a concurrent pair of imports that both passed the read-side check. On conflict it throws `CDuplicateHashException`; the coordinator either rejects or retries via `ForceAdd` depending on `AllowProbableDuplicates`. Empty `sha256_hex` is treated as "hash unknown" and skips the write-side check so books without a computed hash are never false-positives.

`CSingleFileImportCoordinator` computes the SHA-256 of the source file using the Windows BCrypt API (`libs/Hashing`) when the caller does not supply one. This ensures all import paths — single-file, batch, and ZIP — participate in hash-based duplicate detection. If computation fails (I/O error or BCrypt failure) the import continues with an empty hash and a warning is logged; the write-side check is then skipped for that book.

`IBookRepository::ForceAdd` is a separate pure virtual method that inserts without the hash check. It is the explicit override path for callers that have already decided to allow the duplicate.

## 4. Import And Conversion Rules

- `EPUB` is stored as `EPUB`.
- `FB2` is stored as managed `FB2` by default.
- If the current session has a configured converter and the user explicitly enables `Force conversion to EPUB during import`, `FB2` is converted and stored as `EPUB`.
- If forced `FB2 -> EPUB` conversion is requested but the converter is unavailable or fails, the import entry fails rather than silently storing the original `FB2`.
- If original `FB2` is stored as managed fallback, the managed file may be compressed as an internal storage detail; forced `FB2 -> EPUB` import does not apply that compression because the stored format is already `EPUB`.
- Conversion cancellation is not treated as ordinary converter failure.
- `FB2` metadata parsing must preserve non-UTF-8 legacy encodings that still appear in real personal libraries, including Windows-1251 content on Windows.
- duplicates are rejected by default;
- the import UI may explicitly override duplicate rejection and store the new item as a separate managed record.

## 5. Search Model

Search is hybrid:

- structured filters in normal SQLite tables;
- text search through SQLite `FTS5`.

The UI browser is read-side oriented and goes through application facades and transport contracts rather than querying storage directly.

Series and genres are expected to be supported coherently through parser output, persistence, transport contracts, and UI-facing filters/details.

## 6. Build And Repository Layout

### 6.1 Build

- native code uses `CMake` and `vcpkg` manifest mode;
- `CMake` is the canonical native build system;
- managed code uses `.csproj`;
- build artifacts and runtime artifacts belong under repository-root `out/`.

### 6.2 Layout

Current top-level layout:

- `apps/`
- `libs/`
- `proto/`
- `tests/`
- `docs/`
- `scripts/`

Native code is organized as one static library per logical slice under `libs/<SliceName>/`.

## 7. Logging And Testing

### 7.1 Logging

- native logging uses `spdlog`
- UI logging uses `Serilog`
- once a library session is active, both log streams are written under `LibraryRoot/Logs`
- on Windows, native CLI path arguments and native log-file paths must preserve UTF-8 / Unicode library roots instead of falling back to the active ANSI code page

Important runtime paths should log actionable information, especially:

- startup;
- shutdown;
- IPC boundaries;
- long-running jobs;
- failure and recovery paths.

### 7.2 Testing

Testing is layered:

- native `Catch2` tests;
- managed `xUnit` tests;
- targeted integration tests on IPC, storage, SQLite, and host boundaries;
- manual UI validation through a maintained scenario checklist.

## 8. Backlog Reference

Any remaining implementation or hardening work is tracked in:

- [Librova Backlog](Librova-Backlog.md)
