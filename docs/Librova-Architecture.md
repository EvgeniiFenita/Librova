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
- file export and delete-to-trash behavior.

Near-term MVP work still targets three product-facing extensions on top of that baseline:

- recursive directory import;
- series and genres as stronger first-class metadata;
- replacing the current managed delete path with Windows `Recycle Bin` integration.

## 2. IPC Boundary

The MVP transport is:

- `Protobuf` contracts
- over Windows named pipes

This is the canonical runtime boundary between UI and core.

For process lifetime safety on Windows, the UI also passes its parent process id to the native host and binds the host lifetime to the UI session, so the host is not expected to survive a crashed or abruptly terminated UI process.

The MVP deliberately does not depend on:

- `gRPC` runtime
- `P/Invoke` as the main architecture

## 3. Storage And Persistence

### 3.1 Database

- SQLite is the local database.
- Search uses relational filters plus `FTS5` text search.
- Unicode correctness is required across storage and search boundaries.

### 3.2 Managed Library

One managed library root contains:

- `Database`
- `Books`
- `Covers`
- `Logs`
- `Temp`
- `Trash`

Managed paths are stable and `BookId`-based.

The current `Trash` directory is an implementation detail of the present baseline, not a frozen product decision. The active MVP direction is to replace that behavior with a Windows `Recycle Bin` backed delete flow rather than invest in a more complex internal trash or storage-sharding design.

### 3.3 Import Safety

Imports are staged before commit.

The system is expected to:

- avoid partial visible success where possible;
- clean stale temp state on startup;
- keep rollback/failure semantics explicit.

Directory import is expected to follow the same transactional and summary-oriented principles as single-file import, while scaling to recursive scans over mixed-content folders.

## 4. Import And Conversion Rules

- `EPUB` is stored as `EPUB`.
- `FB2` tries to convert to `EPUB`.
- If conversion fails or is unavailable, original `FB2` may be stored with warnings.
- Conversion cancellation is not treated as ordinary converter failure.
- `FB2` metadata parsing must preserve non-UTF-8 legacy encodings that still appear in real personal libraries, including Windows-1251 content on Windows.
- strict duplicates are rejected;
- probable duplicates require explicit user consent.

## 5. Search Model

Search is hybrid:

- structured filters in normal SQLite tables;
- text search through SQLite `FTS5`.

The UI browser is read-side oriented and goes through application facades and transport contracts rather than querying storage directly.

Series and genres are part of the active MVP metadata direction and are expected to be supported coherently through parser output, persistence, transport contracts, and UI-facing filters/details.

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

## 8. Current Architectural Focus

Current architectural focus is:

- adding directory import without weakening import safety guarantees;
- strengthening series/genres support as end-to-end metadata;
- replacing internal trash handling with Windows `Recycle Bin` integration;
- stabilization;
- release hardening;
- runtime review;
- keeping docs and validation current.

## 9. History

Long-form historical planning and architectural evolution are preserved in:

- [Architecture-History](C:\Users\evgen\Desktop\Librova\docs\archive\Architecture-History.md)
