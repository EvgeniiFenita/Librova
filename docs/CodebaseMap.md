# Librova Codebase Map

Navigation reference for agents and contributors working in the Librova repository. Start with `README.md` for the repository-wide documentation map, then use this file to locate modules, boundaries, invariants, and task-entry points.

---

## 0. Reading Guide & Maintenance Rules

### When to read this document

| Situation | Sections to read |
|---|---|
| Starting any task — orient fast | §1 Introduction, §2 Architectural Layers |
| Need to find where a class lives | §3 C++ Modules, §4 Qt/QML App |
| Working on import pipeline | §6 Import Pipeline Architecture |
| Working on storage, DB, covers | §7 Storage & Persistence Model; use `$sqlite` for schema / SQL / FTS work |
| Unsure if a change is safe | §8 Critical Invariants |
| Need to know where to go for a task | §9 Task Navigation |
| Adding or reviewing code style | §11 → `$code-style` skill, `docs/CodeStyleGuidelines.md` |
| Adding a test | §11 Test Conventions, `AGENTS.md` § Verification and test discipline |
| Reviewing domain interfaces | §12 Domain Interfaces Reference |

### Document relationships

This map is a navigation layer, not a full product specification. The authoritative sources are:

| Topic | Authoritative source |
|---|---|
| Repository doc map | `README.md` |
| Frozen architecture decisions | this document, §14 Architecture Decisions |
| Code style (full) | `docs/CodeStyleGuidelines.md` |
| Test policy | `AGENTS.md` § Verification and test discipline |
| UI design system | `docs/UiDesignSystem.md` |
| Product scope | `docs/Librova-Product.md` |
| Active work | `python scripts/backlog.py list` / `python scripts/backlog.py show <id>` |

When this map and an authoritative source disagree, the authoritative source wins. Fix the map in the same task.

### What to update and when

Update `docs/CodebaseMap.md` in the same task when:

| Change | Update which section |
|---|---|
| New `libs/<Module>/` added or renamed | §3 C++ Modules |
| New `apps/Librova.Qt/<Folder>/` added | §4 Qt/QML App |
| Import pipeline stages changed | §6 Import Pipeline Architecture |
| Library root layout or storage encoding changed | §7 Storage & Persistence Model |
| New domain interface added or existing one changed | §12 Domain Interfaces Reference |
| New critical invariant discovered or existing one removed | §8 Critical Invariants |

Do not duplicate decision rationale here outside §14 Architecture Decisions.

---

## 1. Introduction

Librova is a Windows-first desktop e-book library manager. The active application is `LibrovaQtApp`: a one-process Qt/QML shell that composes the native C++ application facade (`libs/App/ILibraryApplication`) directly.

Native C++ owns import, parsing, conversion, persistence, search, collections, export, trash, runtime workspaces, and library bootstrap. Qt/QML controllers, adapters, and models own UI state and user interaction.

Key technologies: CMake + vcpkg, Qt/QML, SQLite + FTS5, spdlog, pugixml, libzip, stb, zlib, Catch2, and BS::thread_pool. All build artifacts are written under repository-root `out/`.

---

## 2. Architectural Layers

| Layer | Location | Responsibility |
|---|---|---|
| **Entry Point** | `apps/Librova.Qt/main.cpp` | Qt startup, CLI parsing, dependency wiring, graceful shutdown |
| **Shell & UI Lifecycle** | `apps/Librova.Qt/App/`, `Controllers/`, `qml/` | Backend worker composition, library root validation, preferences/state persistence, QML shell |
| **Qt Adapters & Models** | `apps/Librova.Qt/Adapters/`, `Models/` | UI-facing catalog/import/export/trash adapters and list/detail models |
| **Application Facade** | `libs/App/` | Orchestrate use cases: import, catalog query, export, trash; async job lifecycle; library bootstrap |
| **Domain** | `libs/Domain/` | Pure value types, interfaces, error types — no I/O, no framework dependencies |
| **Import Pipeline** | `libs/Import/` | Single-file coordinator, parallel ZIP orchestrator, conversion policy, source expansion, diagnostics |
| **Parsing** | `libs/Parsing/` | Format-specific metadata/cover extraction; registry dispatches by format |
| **Persistence** | `libs/Database/` | SQLite repositories, schema migration, FTS5 maintenance, RAII connection wrappers, shared SQL utilities |
| **Managed Storage** | `libs/Storage/` | Stage/commit/rollback book files and covers; sharded object layout; trash workflow; cover processing |
| **Conversion** | `libs/Converter/` | Spawn external FB2→EPUB converter; command building; converter configuration |
| **Infrastructure** | `libs/Foundation/` | UTF-8↔UTF-16 conversions, hashing, logging, version, string and filesystem helpers |

---

## 3. C++ Modules (`libs/`)

### Application & Jobs

| Module | Role | Key types |
|---|---|---|
| `App` | Top-level use-case facade; async job lifecycle; library bootstrap | `ILibraryApplication`, `CLibraryApplication`, `CLibraryImportFacade`, `CLibraryCatalogFacade`, `CLibraryExportFacade`, `CLibraryTrashFacade`, `CImportRollbackService`, `CImportJobService`, `CImportJobManager`, `CImportJobRunner`, `CLibraryBootstrap` |

### Import Pipeline

| Module | Role | Key types |
|---|---|---|
| `Import` | Single-file import coordinator; parallel ZIP import; parallel writer dispatcher; conversion policy; directory/ZIP source expansion; diagnostics | `CSingleFileImportCoordinator`, `CZipImportCoordinator`, `CWriterDispatchingRepository`, `CImportPerfTracker`, `CImportConversionPolicy`, `CImportSourceExpander`, `CImportDiagnosticText`, `CParallelImportHelpers` |

### Parsing

| Module | Role | Key types |
|---|---|---|
| `Parsing` | Parse FB2 and EPUB metadata/covers; genre mapping; parser registry | `CFb2Parser`, `CFb2GenreMapper`, `CEpubParser`, `CBookParserRegistry` |

### Persistence

| Module | Role | Key types |
|---|---|---|
| `Database` | SQLite RAII wrappers; SQL DDL; schema migration; FTS5 maintenance; book and collection repositories | `CSqliteConnection`, `CSqliteStatement`, `CDatabaseSchema`, `CSchemaMigrator`, `CSearchIndexMaintenance`, `CSqliteBookRepository`, `CSqliteBookQueryRepository`, `CSqliteBookCollectionRepository`, `CSqliteTransaction` |

### Managed Storage

| Module | Role | Key types |
|---|---|---|
| `Storage` | Stage/commit/rollback book and cover files; sharded object layout; gz compression; path safety; trash staging; Windows Recycle Bin; cover image processing | `CManagedFileStorage`, `CManagedLibraryLayout`, `CManagedTrashService`, `CWindowsRecycleBinService`, `CStbCoverImageProcessor` |

### Conversion

| Module | Role | Key types |
|---|---|---|
| `Converter` | Spawn external converter process and build command profiles | `CExternalBookConverter`, `CConverterCommandBuilder`, `CConverterConfiguration` |

### Infrastructure

| Module | Role | Key types |
|---|---|---|
| `Domain` | Pure domain types, interfaces, enums, exceptions | `SBook`, `SBookMetadata`, `SBookCollection`, `IBookRepository`, `IBookQueryRepository`, `IBookCollectionRepository`, `IBookCollectionQueryRepository`, `IBookParser`, `IBookConverter`, `IManagedStorage`, `ITrashService`, `IProgressSink`, `CDomainException`, `EDomainErrorCode` |
| `Foundation` | Unicode/path conversion, hashing, logging, version, string and filesystem helpers | `PathFromUtf8`, `PathToUtf8`, `ComputeFileSha256Hex`, `CLogging`, `CVersion`, `ToLower`, `EnsureDirectory`, `RemovePathNoThrow` |

---

## 4. Qt/QML App (`apps/Librova.Qt/`)

| Folder | Role | Key types |
|---|---|---|
| `main.cpp` | Qt entry point, dependency wiring, preferences/state load/save, one-process backend startup | `main` |
| `App/` | Qt runtime glue, CLI parsing, logging, runtime paths, backend dispatcher on a worker thread, Windows integration | `QtApplicationBackend`, `IBackendDispatcher`, `QtLaunchOptions`, `QtRuntimePaths`, `QtRuntimeWorkspaceMaintenance`, `QtLogging`, `QtWindowsPlatform` |
| `Controllers/` | QML-facing shell, first-run, preferences, state, and converter-validation controllers | `ShellController`, `FirstRunController`, `PreferencesStore`, `ShellStateStore`, `ConverterValidationController` |
| `Adapters/` | QML-facing use-case adapters over `ILibraryApplication` | `QtCatalogAdapter`, `QtImportAdapter`, `QtExportAdapter`, `QtTrashAdapter`, `QtStringConversions` |
| `Models/` | Qt list/detail models for catalog, facets, collections, and import jobs | `BookListModel`, `BookDetailsModel`, `CollectionListModel`, `FacetListModel`, `ImportJobListModel` |
| `qml/shell/` | First-run/recovery/ready shell, sidebar, persistent section host | `AppShell.qml`, `SidebarNav.qml`, `SectionLoader.qml`, `FirstRunView.qml`, `StartupRecoveryView.qml` |
| `qml/sections/` | Section wrappers that keep Library/Import/Settings state alive | `LibrarySection.qml`, `ImportSection.qml`, `SettingsSection.qml` |
| `qml/library/` | Library toolbar, grid, cards, details panel, context menu | `LibraryToolbar.qml`, `BookGrid.qml`, `BookCard.qml`, `BookDetailsPanel.qml`, `BookContextMenu.qml` |
| `qml/import/` | Import drop/select workflow, progress/result cards, drop overlay | `ImportView.qml`, `DropZoneOverlay.qml` |
| `qml/settings/` | Converter preferences, about, diagnostics | `SettingsView.qml` |
| `qml/components/`, `qml/theme/` | Shared controls, icons, typography, palette tokens | `LButton`, `LTextInput`, `LCheckBox`, `LNavItem`, `LToast`, `LibrovaTheme`, `LibrovaTypography`, `LibrovaIcons` |

---

## 6. Import Pipeline Architecture

### Import Modes

| Mode | Trigger | Parallelism |
|---|---|---|
| `SingleFile` | One file path | Sequential |
| `ZipArchive` | One `.zip` path | Parallel per entry, up to 8 workers |
| `Batch` | Multiple paths, files and/or directories | ZIP entries parallel; contiguous non-ZIP stretches parallel; ZIP sources sequential |

### Thread Pool Sizing

```
workers = max(1, min(8, hardwareThreads > 1 ? hardwareThreads - 1 : 1))
```

Always at least 1, never above 8, and leaves one thread free for OS and UI responsiveness when possible.

### ZIP Two-Phase Orchestration (`CZipImportCoordinator`)

Phase 1 extracts and submits entries from the main coordinator thread. ZIP handles are not thread-safe, so enumeration remains sequential. Entry bytes are staged under `working_dir/entries/<zip_index>/`, never under user-controlled archive paths.

Phase 2 drains worker completion notifications, waits for all futures, and materializes results in original ZIP order. Result ordering is stable even when workers finish out of order.

### Import Cancellation

Cancellation is not converter failure. If cancellation happens after books were committed, `CLibraryImportFacade` collects imported IDs and calls `CImportRollbackService::RollbackImportedBooks()`. The terminal result reports cancellation and rollback explicitly.

---

## 7. Storage & Persistence Model

Managed library layout:

| Path | Purpose |
|---|---|
| `<LibraryRoot>/Database/librova.db` | SQLite database |
| `<LibraryRoot>/Objects/` | Managed book and cover objects |
| `<LibraryRoot>/Logs/` | Retained diagnostics for the active library |
| `<LibraryRoot>/Trash/` | Managed trash fallback and rollback staging |

Hot runtime workspaces are outside the durable library root by default. Import workspaces, converter working directories, and managed-storage staging live under the runtime workspace computed by `QtRuntimePaths` / `CLibraryApplication`.

Database schema version is currently `2`. `CSchemaMigrator` accepts:

- `user_version == 0`: create fresh schema and set version to `2`
- `user_version == 1`: deliberate non-destructive upgrade for collection infrastructure
- `user_version == 2`: no-op

Any other version is incompatible and must throw.

Managed FB2 files may be stored as `.book.fb2.gz` under `Objects/` as an internal storage detail. Browse, export, delete, and duplicate detection treat them as ordinary FB2 books.

---

## 8. Critical Invariants

1. **Qt-only runtime.** User workflows run through `LibrovaQtApp` and `ILibraryApplication` in one process.
2. **Domain logic stays out of QML.** QML composes views and calls Qt controllers/adapters.
3. **`PathFromUtf8()` for UTF-8 paths.** Never construct `std::filesystem::path` from UTF-8 `std::string` on Windows.
4. **Schema version policy is explicit.** Do not add automatic migrations unless the change is deliberately non-destructive.
5. **Cancellation is explicit.** It is never silently treated as converter failure or fallback storage.
6. **ZIP entry names are normalized before path conversion.** Legacy CP866 archive names must become UTF-8 before safe path handling.
7. **Futures are never abandoned.** ZIP import resolves all worker futures even after cancellation.
8. **Export replacement is staged.** Export writes to a temporary sibling path first so failures do not corrupt an existing user file.
9. **UI/backend boundary logging is mandatory.** Qt adapters log successful outcomes and route backend exceptions to explicit error paths.
10. **Build artifacts stay under `out/`.** No project-local generated `bin/` or `obj/` directories are steady state.

---

## 9. Task Navigation

### Change single-file import logic

- `libs/Import/SingleFileImportCoordinator.cpp/.hpp`
- `libs/Import/ImportConversionPolicy.cpp/.hpp`
- `libs/Import/ImportDiagnosticText.cpp`
- Tests: `tests/Unit/TestSingleFileImportCoordinator.cpp`

### Change parallel import

- ZIP: `libs/Import/ZipImportCoordinator.cpp/.hpp`
- Batch planning: `libs/App/LibraryImportFacade.cpp`, `libs/App/ImportWorkloadPlanner.cpp`, `libs/Import/ParallelImportHelpers.hpp`
- Writer: `libs/Import/WriterDispatchingRepository.cpp/.hpp`
- Tests: `tests/Unit/TestZipImportCoordinator.cpp`, `tests/Unit/TestLibraryImportFacade.cpp`, `tests/Unit/TestWriterDispatchingRepository.cpp`

### Add a new book format

- Implement `IBookParser` in `libs/Parsing/`
- Extend `EBookFormat` in `libs/Domain/BookFormat.hpp`
- Update import, storage, export, and Qt adapter/model mappings
- Add parser and end-to-end application tests

### Change the database schema

Use the `$sqlite` skill before editing.

- SQL DDL: `libs/Database/DatabaseSchema.cpp`
- Version policy: `libs/Database/SchemaMigrator.cpp`
- Repository/query updates: `libs/Database/SqliteBookRepository.cpp`, `SqliteBookQueryRepository.cpp`
- Tests: `tests/Unit/TestSchemaMigrator.cpp`, `TestDatabaseSchema.cpp`, relevant repository tests

### Add a field to the book card

1. Add field to `SBookMetadata` or the appropriate domain DTO.
2. Parse it in `CFb2Parser` and/or `CEpubParser`.
3. Persist it in `libs/Database/`.
4. Expose it through `IBookQueryRepository`, `CLibraryCatalogFacade`, Qt adapters/models.
5. Bind it in QML.
6. Add unit, adapter/model, and QML structural tests as appropriate.

### Change delete / Recycle Bin logic

- Facade: `libs/App/LibraryTrashFacade.cpp`
- Trash staging: `libs/Storage/ManagedTrashService.cpp`
- Recycle Bin: `libs/Storage/WindowsRecycleBinService.cpp`
- Qt adapter: `apps/Librova.Qt/Adapters/QtTrashAdapter.cpp`

### Add or change a Qt screen / workflow

- QML section/view: `apps/Librova.Qt/qml/sections/` or a domain folder under `qml/`
- Controller/adapter/model: `apps/Librova.Qt/Controllers/`, `Adapters/`, `Models/`
- Keep business rules in C++ application/domain layers or Qt adapters
- Tests: `tests/Librova.Qt.Tests/`
- Styles: reuse `LibrovaTheme`, `LibrovaTypography`, shared QML controls, and `docs/UiDesignSystem.md`

### Change converter settings

- Configuration and command building: `libs/Converter/`
- External process execution: `libs/Converter/ExternalBookConverter.cpp`
- Qt validation/preferences: `apps/Librova.Qt/Controllers/ConverterValidationController.*`, `PreferencesStore.*`, `qml/settings/SettingsView.qml`

### Add a unit test

- C++ test file: `tests/Unit/Test<Area>.cpp`
- Framework: Catch2 (`TEST_CASE`, `REQUIRE`, `SECTION`)
- Temp database: call `repository.CloseSession()` in teardown before deleting the file

### Add a Qt test

- File: `tests/Librova.Qt.Tests/Test<Area>.cpp`
- Framework: Qt Test (`QTEST_*_MAIN`, `QCOMPARE`, `QVERIFY`, `QSignalSpy`)
- Register production Qt sources in `tests/Librova.Qt.Tests/CMakeLists.txt`
- Strong one-process flows belong in `TestQtStrongIntegration.cpp`

---

## 10. External Dependencies

| Library | Used in | Purpose |
|---|---|---|
| `pugixml` | `libs/Parsing/` | XML parsing for FB2 and EPUB OPF metadata |
| `libzip` | `libs/Import/` | ZIP archive enumeration and entry extraction |
| `sqlite3` (+ fts5 feature) | `libs/Database/` | Embedded relational database and FTS5 search |
| `spdlog` | `libs/Foundation/`; all C++ modules | Structured logging |
| `stb` | `libs/Storage/` | Image decode/encode for cover processing |
| `bshoshany-thread-pool` | `libs/Import/` | Fixed-size thread pool for parallel import workers |
| `zlib` | `libs/Storage/`; libzip dependency | gz compression/decompression for internal FB2 storage |
| `catch2` | `tests/Unit/` | C++ unit test framework |
| `Qt 6` | `apps/Librova.Qt/`, `tests/Librova.Qt.Tests/` | Active QML UI, controllers, models, dialogs, and Qt tests |

---

## 11. Code And Test Conventions

Authoritative style source: `docs/CodeStyleGuidelines.md` and the `$code-style` skill.

- C++ tests use Catch2 and names that describe observable behavior.
- Qt tests use Qt Test and register production source files through `tests/Librova.Qt.Tests/CMakeLists.txt`.
- Mandatory regression areas include Unicode/Cyrillic paths, legacy encodings such as `windows-1251`, duplicate detection, rollback after cancellation, export staging, and Qt/native adapter state transitions.
- Native tests that open persistent SQLite repositories must call `CloseSession()` before deleting database files.

---

## 12. Domain Interfaces Reference

Quick reference for core abstractions in `libs/Domain/`. All are implemented in higher layers and consumed by application/import/storage components.

### `IBookRepository` (write)

```cpp
SBookId ReserveId();
std::vector<SBookId> ReserveIds(size_t count);
SBookId Add(const SBook& book);
SBookId ForceAdd(const SBook& book);
std::optional<SBook> GetById(SBookId id) const;
void Remove(SBookId id);
void Compact(const std::function<void()>& onProgressTick = nullptr);
void OptimizeSearchIndex();
void RemoveBatch(std::span<const SBookId> ids);
std::vector<SBatchBookResult> AddBatch(std::span<const SBatchBookEntry> entries);
```

### `IBookQueryRepository` (read)

```cpp
std::vector<SBook> Search(const SSearchQuery& query) const;
std::uint64_t CountSearchResults(const SSearchQuery& query) const;
std::vector<SFacetItem> ListAvailableLanguages(const SSearchQuery& query) const;
std::vector<std::string> ListAvailableTags(const SSearchQuery& query) const;
std::vector<SFacetItem> ListAvailableGenres(const SSearchQuery& query) const;
std::vector<SDuplicateMatch> FindDuplicates(const SCandidateBook& candidate) const;
SLibraryStatistics GetLibraryStatistics() const;
```

### `IBookCollectionRepository` / `IBookCollectionQueryRepository`

```cpp
std::vector<SBookCollection> ListCollections() const;
std::vector<SBookCollection> ListCollectionsForBook(SBookId bookId) const;
std::unordered_map<std::int64_t, std::vector<SBookCollection>> ListCollectionsForBooks(
    const std::vector<SBookId>& bookIds) const;
std::optional<SBookCollection> GetCollectionById(std::int64_t collectionId) const;
SBookCollection CreateCollection(std::string nameUtf8, std::string iconKey);
bool DeleteCollection(std::int64_t collectionId);
bool AddBookToCollection(SBookId bookId, std::int64_t collectionId);
bool RemoveBookFromCollection(SBookId bookId, std::int64_t collectionId);
```

### `IProgressSink`

```cpp
void ReportValue(int percent, std::string_view message);
bool IsCancellationRequested() const;
void BeginRollback(std::size_t totalToRollback);
void ReportRollbackProgress(std::size_t rolledBack, std::size_t total);
void BeginCompacting();
```

### `IManagedStorage`

```cpp
SPreparedStorage PrepareImport(const SStoragePlan& plan);
void CommitImport(const SPreparedStorage& prepared);
void RollbackImport(const SPreparedStorage& prepared) noexcept;
```

### `IBookParser`

```cpp
bool CanParse(EBookFormat format) const;
SParsedBook Parse(
    const std::filesystem::path& path,
    std::string_view logicalSourceLabel,
    const SBookParseOptions& options) const;
```

### `IBookConverter`

```cpp
bool CanConvert(EBookFormat source, EBookFormat dest) const;
SConversionResult Convert(
    const SConversionRequest& request,
    IProgressSink& progressSink,
    std::stop_token stopToken) const;
```

---

## 13. Process Startup & Shutdown

### Qt App (`apps/Librova.Qt/main.cpp`)

1. Parse Qt launch options and initialize Qt logging.
2. Load `PreferencesStore` and `ShellStateStore`.
3. Create `QtApplicationBackend`, which owns `ILibraryApplication` on a worker thread and reports opened/openFailed state to the GUI thread.
4. Create QML-facing controllers and adapters: shell, first-run validation, catalog, import, export, trash, converter validation, and Windows platform helpers.
5. Load `Main.qml`; `ShellController` chooses first-run, recovery, damaged-library, opening, or ready shell state.
6. In ready state, QML sections call adapters; adapters dispatch to the in-process C++ facade and surface explicit success/error callbacks.
7. On exit, save preferences/state, sync logs, close the backend on its worker thread, and flush logging.

---

## 14. Architecture Decisions

### 14.1 Runtime Model

Librova's runtime is a one-process desktop application.

`LibrovaQtApp` (Qt/QML + C++) owns:

- windowing and user interactions
- desktop dialogs
- QML section lifecycle and UI-facing models
- preferences, shell state, first-run/recovery state, and runtime path presentation

Native C++ (`libs/App` and lower layers) owns:

- domain logic
- import pipeline
- parsing and format handling
- conversion orchestration
- persistence (SQLite + FTS5)
- managed storage
- async job lifecycle
- search
- file export and staged delete behavior

The UI must call native application use cases through `ILibraryApplication` and Qt adapters, not by duplicating business logic in QML.

### 14.2 Boundary Model

Normal user workflows do not cross a process boundary. The enforced boundary is a code ownership boundary:

- QML views call Qt controllers/adapters.
- Qt adapters call `ILibraryApplication` through `QtApplicationBackend`.
- `ILibraryApplication` composes native application facades and lower-level domain services.

### 14.3 Storage Design

`CSqliteBookRepository` keeps a single SQLite write connection open for the repository lifetime, protected by a mutex-serialized write session. In tests, call `repository.CloseSession()` before deleting the database file.

Import safety principles:

- avoid partial visible success where possible
- clean stale runtime workspace state on startup
- keep rollback and failure semantics explicit

Library-root bootstrap modes:

- `Create Library` may initialize only a new or empty target directory
- `Open Library` must validate that the selected root is already a complete managed library, including `Database/librova.db`

---

See also: `docs/CodeStyleGuidelines.md` and `docs/UiDesignSystem.md`.
