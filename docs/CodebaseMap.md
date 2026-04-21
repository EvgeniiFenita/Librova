# Librova Codebase Map

Navigation reference for agents and contributors working in the Librova repository. Start with `README.md` for the repository-wide documentation map, then use this file to locate modules, boundaries, invariants, and task-entry points.

---

## 0. Reading Guide & Maintenance Rules

### When to read this document

| Situation | Sections to read |
|---|---|
| Starting any task — orient fast | §1 Introduction, §2 Architectural Layers |
| Need to find where a class lives | §3 C++ Modules, §4 C# Modules |
| Working on IPC / proto | §5 IPC Boundary, then `$transport-rpc` skill |
| Working on import pipeline | §6 Import Pipeline Architecture |
| Working on storage, DB, covers | §7 Storage & Persistence Model; use `$sqlite` for schema / SQL / FTS work |
| Unsure if a change is safe | §8 Critical Invariants |
| Need to know where to go for a task | §9 Task Navigation |
| Adding or reviewing code style | §11 → `$code-style` skill, `docs/CodeStyleGuidelines.md` |
| Adding a test | §11 Test Conventions, `AGENTS.md` § Verification and test discipline |
| Reviewing domain interfaces | §12 Domain Interfaces Reference |

### Document relationships

This map is a **navigation layer**, not an authoritative specification. The authoritative sources are:

| Topic | Authoritative source |
|---|---|
| Repository doc map | `README.md` |
| Frozen architecture decisions | this document, §14 Architecture Decisions |
| IPC contract rules | this document, §5 IPC Boundary (IPC Invariants subsection) |
| Code style (full) | `docs/CodeStyleGuidelines.md` |
| Test policy | `AGENTS.md` § Verification and test discipline |
| UI design system | `docs/UiDesignSystem.md` |
| Product scope | `docs/Librova-Product.md` |
| Active work | `python scripts/backlog.py list` / `python scripts/backlog.py show <id>` |

When this map and an authoritative source disagree, **the authoritative source wins**. Fix the map.

### What to update and when

Update `docs/CodebaseMap.md` in the **same task** (not later) when:

| Change | Update which section |
|---|---|
| New `libs/<Module>/` added or renamed | §3 C++ Modules — add/update the row |
| New `apps/Librova.UI/<Folder>/` added | §4 C# Modules — add/update the row |
| New RPC method added to proto | §5 IPC Boundary — add row to the methods table |
| Import pipeline stages changed | §6 Import Pipeline Architecture |
| Library root layout or storage encoding changed | §7 Storage & Persistence Model |
| New domain interface added or existing one changed | §12 Domain Interfaces Reference |
| New critical invariant discovered or existing one removed | §8 Critical Invariants |

Do **not** duplicate decision rationale here — that belongs in §14 Architecture Decisions.

---

## 1. Introduction

Librova is a Windows-first desktop e-book library manager. It is structured as **two cooperating processes**: `Librova.Core` (C++20 native host) handles all domain logic — import, parsing, conversion, persistence, search, export, and trash — while `Librova.UI` (C# / .NET / Avalonia) owns the UI, ViewModels, and user interactions. The processes are deployed as a matched pair; no cross-version named-pipe compatibility is guaranteed.

The **IPC boundary** is a Windows named pipe carrying length-prefixed Protobuf 3 messages. The contract is defined in a single `.proto` file (`proto/import_jobs.proto`) and exposes 11 RPC methods via the `LibraryJobService` service. There is no gRPC runtime and no P/Invoke as the primary transport. The UI process spawns the native host on startup and controls its lifetime; the host can also self-terminate if the parent UI PID disappears.

Key technologies: CMake + vcpkg (native build), .csproj / MSBuild (managed build), SQLite + FTS5 (persistence and full-text search), spdlog (C++ logging), Serilog (C# logging), pugixml (XML / FB2 parsing), libzip (ZIP extraction), stb (cover image processing), BS::thread_pool (parallel import). All build artifacts are written to `out/`.

---

## 2. Architectural Layers

| Layer | Location | Responsibility |
|---|---|---|
| **Entry Points** | `apps/Librova.Core.Host/Main.cpp`, `apps/Librova.UI/Program.cs` | Process startup, CLI parsing, dependency wiring, graceful shutdown |
| **Shell & UI Lifecycle** | `apps/Librova.UI/Shell/`, `apps/Librova.UI/CoreHost/` | Bootstrap, library root validation, host process spawn/teardown, preferences persistence |
| **ViewModels** | `apps/Librova.UI/ViewModels/` | MVVM logic: browse, import job tracking, first-run wizard, settings, section state machine |
| **Views** | `apps/Librova.UI/Views/`, `apps/Librova.UI/Styles/` | AXAML markup, code-behind event handlers, design-token styles |
| **Transport (C#)** | `apps/Librova.UI/PipeTransport/`, `apps/Librova.UI/ImportJobs/`, `apps/Librova.UI/LibraryCatalog/` | Named-pipe client, envelope protocol, service wrappers, proto ↔ C# model mapping |
| **IPC Boundary** | `proto/import_jobs.proto` | Canonical contract: 11 RPC methods, all message types, all enums |
| **Transport (C++)** | `libs/Transport/` | Named-pipe host, envelope serialization/deserialization, request dispatcher, pipe client |
| **Proto Adapter** | `libs/Rpc/` | Route pipe method IDs → facade calls; translate proto ↔ domain types |
| **Application Facades** | `libs/App/` | Orchestrate use cases: import, catalog query, export, trash; async job lifecycle; host bootstrap and CLI options |
| **Domain** | `libs/Domain/` | Pure value types, interfaces, error types — no I/O, no framework dependencies |
| **Import Pipeline** | `libs/Import/` | Single-file coordinator, parallel ZIP orchestrator, conversion policy, source expansion, diagnostics |
| **Parsing** | `libs/Parsing/` | Format-specific metadata/cover extraction; registry dispatches by format |
| **Persistence** | `libs/Database/` | SQLite repositories, schema migration, FTS5 maintenance, RAII connection wrappers, shared SQL utilities (`SqliteEntityHelpers`, `SqliteTimePoint`, `SqliteTransaction`) |
| **Managed Storage** | `libs/Storage/` | Stage/commit/rollback book files and covers; sharded object layout; trash workflow; cover decode/resize/re-encode |
| **Conversion** | `libs/Converter/` | Spawn external FB2→EPUB converter; cover decode/resize/re-encode |
| **Infrastructure** | `libs/Foundation/` | UTF-8↔UTF-16 conversions, SHA-256 (BCrypt), spdlog init, version constant, shared string utilities (`StringUtils`), shared filesystem utilities (`FileSystemUtils`) |

---

## 3. C++ Modules (`libs/`)

### IPC & Transport

| Module | Role | Key types |
|---|---|---|
| `Transport` | Named-pipe channel I/O, envelope protocol (serialize/deserialize `SPipeRequestEnvelope` / `SPipeResponseEnvelope`), request dispatcher, pipe server, pipe client | `CNamedPipeChannel`, `CPipeProtocol`, `CPipeRequestDispatcher`, `CNamedPipeHost`, `CNamedPipeClient`, `EPipeMethod` |
| `Rpc` | Generated Protobuf C++ code from `proto/import_jobs.proto`; translate proto ↔ domain structs; route method enum → facade/manager calls with mandatory outcome logging | all `librova::v1::*` message classes, `CImportJobProtoMapper`, `CLibraryCatalogProtoMapper`, `CLibraryJobServiceAdapter` |
| `ApplicationClient` | C++ import job client (wraps pipe channel; used in native integration tests) | `CImportJobClient` |

### Application & Jobs

| Module | Role | Key types |
|---|---|---|
| `App` | Top-level use-case facades; async job lifecycle; host CLI options and library bootstrap | `CLibraryImportFacade`, `CLibraryCatalogFacade`, `CLibraryExportFacade`, `CLibraryTrashFacade`, `CImportRollbackService`, `CImportJobService`, `CImportJobManager`, `CImportJobRunner`, `CHostOptions`, `CLibraryBootstrap` |

### Import Pipeline

| Module | Role | Key types |
|---|---|---|
| `Import` | Single-file import coordinator; parallel ZIP import; parallel writer dispatcher; performance tracker; conversion policy (FB2→EPUB); directory/ZIP source expansion; diagnostics | `CSingleFileImportCoordinator`, `CZipImportCoordinator`, `CWriterDispatchingRepository`, `CImportPerfTracker`, `CImportConversionPolicy`, `CImportSourceExpander`, `CImportDiagnosticText`, `CParallelImportHelpers` |

### Parsing

| Module | Role | Key types |
|---|---|---|
| `Parsing` | Parse FB2 (Windows-1251 decode) and EPUB (OPF/ZIP); genre mapping; dispatch `IBookParser` by format | `CFb2Parser`, `CFb2GenreMapper`, `CEpubParser`, `CBookParserRegistry` |

### Persistence

| Module | Role | Key types |
|---|---|---|
| `Database` | SQLite RAII wrappers; SQL DDL constants; schema migration (v0→1); FTS5 index maintenance; SQLite book repositories; genre helpers; shared SQL utilities in `Librova::Sqlite` namespace (`BuildIdInClause`, `ResolveEntityId`, `ReadRelatedEntityNames`, `ParseTimePoint`, `SerializeTimePoint`, `CSqliteTransaction`) | `CSqliteConnection`, `CSqliteStatement`, `CDatabaseSchema`, `CSchemaMigrator`, `CSearchIndexMaintenance`, `CSqliteBookRepository` (write, `IBookRepository`), `CSqliteBookQueryRepository` (read, `IBookQueryRepository`), `CSqliteGenreHelpers` |

### Managed Storage

| Module | Role | Key types |
|---|---|---|
| `Storage` | Stage/commit/rollback book and cover files; sharded object layout; gz-compress/decompress; path safety; trash staging; Windows Recycle Bin; cover decode/resize/re-encode | `CManagedFileStorage` (implements `IManagedStorage`), `CManagedLibraryLayout`, `CManagedTrashService` (implements `ITrashService`), `CWindowsRecycleBinService` (implements `IRecycleBinService`), `CStbCoverImageProcessor` (implements `ICoverImageProcessor`) |

### Conversion

| Module | Role | Key types |
|---|---|---|
| `Converter` | Spawn external converter process (fb2cng / ebook-convert); build CLI command string; load and validate converter binary path from settings | `CExternalBookConverter` (implements `IBookConverter`), `CConverterCommandBuilder`, `CConverterConfiguration` |

### Infrastructure

| Module | Role | Key types |
|---|---|---|
| `Domain` | All pure domain types: value objects, interfaces, enums, exceptions — zero I/O dependencies | `SBook`, `SBookId`, `SBookMetadata`, `SBookFileInfo`, `SParsedBook`, `SBookParseOptions`, `IBookRepository`, `IBookQueryRepository`, `IBookParser`, `IBookConverter`, `IManagedStorage`, `ITrashService`, `IProgressSink`, `CDomainException`, `CDuplicateHashException`, `EBookFormat`, `EStorageEncoding`, `EDomainErrorCode` |
| `Foundation` | UTF-8 ↔ UTF-16 conversions (`PathFromUtf8()` — the **only** approved way to create `std::filesystem::path` from UTF-8 strings); SHA-256 via Windows BCrypt API; spdlog initialization and `*IfInitialized` log helpers; compile-time version constant; `ToLower` string utility; `EnsureDirectory` / `RemovePathNoThrow` filesystem helpers | `PathFromUtf8()`, `ComputeFileSha256Hex()`, `CLogging`, `CVersion`, `ToLower`, `EnsureDirectory`, `RemovePathNoThrow` |
| `CoreHost` | Parse CLI options; bootstrap library (schema migration, service wiring) | `CLibraryBootstrap`, `SHostOptions` |

---

## 4. C# Modules (`apps/Librova.UI/`)

| Folder | Role | Key types |
|---|---|---|
| `Program.cs` / `App.axaml.cs` | Avalonia entry point; top-level exception handler | `App` |
| `CoreHost/` | Spawn and manage native host process; resolve executable path; host launch defaults | `CoreHostProcess`, `CoreHostPathResolver`, `CoreHostLaunchOptions`, `CoreHostDevelopmentDefaults`, `UiConverterMode` |
| `Shell/` | Application bootstrap; session lifecycle; preferences and state persistence; library root validation | `ShellApplication`, `ShellBootstrap`, `ShellSession`, `ShellStateStore`, `UiPreferencesStore`, `LibraryRootValidation`, `FirstRunSetupPolicy`, `ConverterValidationCoordinator`, `Fb2ConverterProbe` |
| `ViewModels/` | All MVVM ViewModel classes | `ShellViewModel`, `ShellWindowViewModel`, `LibraryBrowserViewModel`, `ImportJobsViewModel`, `FirstRunSetupViewModel`, `ShellConverterPathController`, `ShellImportWorkflowController`, `ShellLibrarySwitchController`, `LibraryCoverPresentationService`, `FilterFacetItem`, `EmptyStateKind` |
| `Views/` | AXAML markup + code-behind | `MainWindow`, `LibraryView`, `ImportView`, `SettingsView` |
| `Styles/` | Design-token resources | `Colors.axaml`, `Typography.axaml`, `Components.axaml` |
| `ImportJobs/` | Import job IPC client + service + mapper + models | `IImportJobsService`, `ImportJobsService`, `ImportJobClient`, `ImportJobMapper`, `ImportJobModels`, `ImportJobDomainError` |
| `LibraryCatalog/` | Catalog query IPC client + service + mapper + models | `ILibraryCatalogService`, `LibraryCatalogService`, `LibraryCatalogClient`, `LibraryCatalogMapper`, `LibraryCatalogModels`, `FacetItemModel`, `NullLibraryCatalogService` |
| `PipeTransport/` | Low-level named-pipe I/O and envelope protocol (mirrors C++ side) | `NamedPipeClient`, `PipeProtocol` |
| `Runtime/` | Version, OS environment, workspace cleanup, log sync | `ApplicationVersion`, `RuntimeEnvironment`, `RuntimeWorkspaceMaintenance`, `RuntimeLogSynchronization` |
| `Mvvm/` | Base MVVM helpers | `AsyncCommand`, `ObservableObject` |
| `Desktop/` | File/folder picker dialogs | `IPathSelectionService`, `AvaloniaPathSelectionService`, `NullPathSelectionService` |
| `Logging/` | Serilog initialization | `LoggingInitializer` |
| `Styling/` | Runtime theme resource management | `UiThemeResources` |

---

## 5. IPC Boundary

### Contract: `LibraryJobService` (`proto/import_jobs.proto`)

| Method | Request → Response | Purpose |
|---|---|---|
| `StartImport` | `StartImportRequest → StartImportResponse` | Launch async import job; returns `job_id` |
| `ValidateImportSources` | `ValidateImportSourcesRequest → ValidateImportSourcesResponse` | Pre-flight check; returns optional `blocking_message` |
| `GetImportJobSnapshot` | `GetImportJobSnapshotRequest → GetImportJobSnapshotResponse` | Current job progress (status, percent, counters, warnings) |
| `WaitImportJob` | `WaitImportJobRequest → WaitImportJobResponse` | Long-poll with timeout; unblocks when job is terminal |
| `GetImportJobResult` | `GetImportJobResultRequest → GetImportJobResultResponse` | Full terminal result: summary, per-file outcomes, errors |
| `CancelImportJob` | `CancelImportJobRequest → CancelImportJobResponse` | Request cancellation; job transitions to Cancelling → RollingBack → Compacting → Cancelled |
| `RemoveImportJob` | `RemoveImportJobRequest → RemoveImportJobResponse` | Discard completed job record from manager |
| `ListBooks` | `ListBooksRequest → ListBooksResponse` | Query catalog with filters, sort, pagination |
| `GetBookDetails` | `GetBookDetailsRequest → GetBookDetailsResponse` | Full metadata for a single book |
| `ExportBook` | `ExportBookRequest → ExportBookResponse` | Copy/convert managed book to destination path |
| `MoveBookToTrash` | `MoveBookToTrashRequest → MoveBookToTrashResponse` | Remove from catalog; route to Recycle Bin or managed trash |

### Protocol Wire Format

- Every call is wrapped in `SPipeRequestEnvelope` (request_id, method `EPipeMethod`, payload bytes) / `SPipeResponseEnvelope` (request_id, status, error_message, payload bytes).
- Method IDs in `EPipeMethod` are **append-only integers** (1–10, 12; ID 11 is reserved). Adding a method requires a new ID; IDs are never recycled.
- UI and Core are deployed in lockstep; no backward-compatibility guarantee across versions.

### Mapping Points

| Side | Where mapping happens | Types |
|---|---|---|
| C++ inbound | `libs/Rpc/` | `CImportJobProtoMapper`, `CLibraryCatalogProtoMapper` — proto message → domain struct |
| C++ outbound | same mappers | domain struct → proto message |
| C# inbound | `apps/Librova.UI/ImportJobs/ImportJobMapper.cs` | proto → `ImportJobSnapshotModel`, `ImportJobResultModel` |
| C# inbound | `apps/Librova.UI/LibraryCatalog/LibraryCatalogMapper.cs` | proto → `BookListPageModel`, `BookDetailsModel` |

### Key Message Types

**`StartImportRequest`** fields: `source_paths[]`, `working_directory`, optional `sha256_hex`, `allow_probable_duplicates` (bool), `force_epub_conversion` (bool), `import_covers` (optional bool, default true — when false, the pipeline skips cover extraction and storage for the entire batch).

**`ImportJobSnapshot`** fields: `job_id`, `status` (`ImportJobStatus` enum), `percent` (0–100), `total_entries`, `processed_entries`, `imported_entries`, `failed_entries`, `skipped_entries`, `warnings[]`.

**`ImportJobStatus` enum**: `PENDING`, `RUNNING`, `COMPLETED`, `FAILED`, `CANCELLED`, `CANCELLING`, `ROLLING_BACK`, `COMPACTING` — 8 states, full lifecycle including post-cancel rollback phases.

**`BookListItem`** fields: `id`, `title`, `authors[]`, `language`, `series`, `series_index`, `publisher`, `year`, `isbn`, `tags[]`, `genres[]`, `format` (`BookFormat` enum), `managed_file_name`, `cover_resource_available`, `cover_file_extension`, `cover_relative_path`, `size_bytes`, `added_at_unix_ms`.

**`BookDetails`** fields: `id`, `title`, `authors[]`, `language`, `series`, `series_index`, `publisher`, `year`, `isbn`, `description`, `identifier`, `tags[]`, `genres[]`, `format` (`BookFormat` enum), `managed_file_name`, `cover_resource_available`, `content_hash_available`, `cover_file_extension`, `cover_relative_path`, `size_bytes`, `added_at_unix_ms`.

**`BookListRequest`** fields: `text` (FTS query), `author`, `languages[]`, `genres[]`, `series`, `tags[]`, `format`, `sort_by` (`BookSort` enum), `direction` (`SortDirection`), `offset`, `limit`.

**`ErrorCode`** enum values: `VALIDATION`, `UNSUPPORTED_FORMAT`, `DUPLICATE_REJECTED`, `PARSER_FAILURE`, `CONVERTER_UNAVAILABLE`, `CONVERTER_FAILED`, `STORAGE_FAILURE`, `DATABASE_FAILURE`, `CANCELLATION`, `INTEGRITY_ISSUE`, `NOT_FOUND`.

**`BookFormat`** enum: `EPUB`, `FB2`, `ZIP`.  
**`DeleteDestination`** enum: `RECYCLE_BIN`, `MANAGED_TRASH`.

### Ordered checklist for a new RPC

1. Add message types + method to `proto/import_jobs.proto`
2. Regenerate C++ proto code (`libs/Rpc/`)
3. Add method ID to `EPipeMethod` enum (append only)
4. Register in `CPipeRequestDispatcher`
5. Implement in `CLibraryJobServiceAdapter` (with mandatory outcome logging)
6. Add mapping in `CImportJobProtoMapper` or `CLibraryCatalogProtoMapper`
7. Add facade method (if new domain operation)
8. Add C# pipe method enum value
9. Implement in `ImportJobClient` or `LibraryCatalogClient`
10. Add service method in `ImportJobsService` / `LibraryCatalogService` (with success-path logging)
11. Add mapper in `ImportJobMapper` / `LibraryCatalogMapper`
12. Run `scripts/ValidateProto.ps1`; rebuild both sides; run all tests

### IPC Invariants

**Protobuf rules:**
- Treat `proto/import_jobs.proto` as append-only; never reuse field numbers.
- Prefer additive evolution over breaking reshapes.
- Keep DTOs transport-oriented, not storage-oriented.

**Named-pipe method rules:**
- `Librova.UI` and `Librova.Core` are deployed in lockstep; cross-version named-pipe compatibility between checkpoints is not a supported runtime contract.
- Within one checkpoint, native and managed named-pipe method IDs must match exactly.
- When a method is replaced or removed, update both sides in the same checkpoint; keep the change explicit in tests and docs.
- Every new method must appear in both native `PipeProtocol` and C# `PipeProtocol` enums.
- Every new method must be accepted by parser/validation logic on both sides.

---

## 6. Import Pipeline Architecture

### Import Modes

| Mode | Trigger | Parallelism |
|---|---|---|
| `SingleFile` | One file path | Sequential |
| `ZipArchive` | One `.zip` path | Parallel (per-entry, up to 8 workers) |
| `Batch` | Multiple paths (files and/or dirs) | ZIP entries parallel; contiguous non-ZIP stretches parallel; ZIP sources sequential |

### Thread Pool Sizing

```
workers = max(1, min(8, hardwareThreads > 1 ? hardwareThreads - 1 : 1))
```

Always ≥ 1 (prevents deadlock on single-core); ≤ 8 (prevents I/O saturation); leaves 1 thread free for OS + Avalonia UI.

### ZIP Two-Phase Orchestration (`CZipImportCoordinator`)

**Phase 1 — Extract & Submit (main thread only):**
1. Iterate ZIP entries sequentially (`zip_t*` is not thread-safe).
2. Skip immediately (no I/O, no semaphore): directories, nested archives, unsafe paths, unsupported formats.
3. Check cancellation + semaphore back-pressure (`inFlight.acquire()` — blocks main thread if temp space full).
4. Extract entry bytes to `working_dir/entries/<zip_index>/` (index, not filename).
5. Submit worker lambda to thread pool; store `std::future` in slot vector at original entry index.

**Phase 2 — Finalise in Order (main thread):**
1. Drain completion notifications from progress queue (workers push as they finish).
2. Wait for all submitted futures (`future.get()`).
3. Iterate slots in original ZIP order: materialise results preserving entry ordering.

Ordering guarantee: result order == ZIP entry order, despite parallel worker completion.

### Back-Pressure Semaphore

- Capacity: `threadCount × 2`.
- Main thread acquires before extraction; worker releases on completion (RAII guard).
- Peak temp disk usage ≈ `threadCount × 2 × ~500 KB` (8–16 MB, not GB).

### Writer Dispatcher (`CWriterDispatchingRepository`)

- Single dedicated writer thread; all `Add()` / `ForceAdd()` calls routed through it.
- Callers (workers) submit a request and get a `std::future`; block on `future.get()`.
- Writer batches up to **50 requests per SQLite transaction** — reduces per-book overhead.
- Read operations (`ReserveId`, `GetById`, `Remove`, `Compact`) forwarded directly to underlying repository (no queuing).
- Fatal writer error stops all further submissions; pending futures unblock with error status.

### Single-File Import Stages (`CSingleFileImportCoordinator`)

1. Parse metadata + cover (`CBookParserRegistry`)
2. Find duplicates — read-side check (`IBookQueryRepository::FindDuplicates`)
3. Compute SHA-256 (`CSha256`)
4. Plan conversion (`CImportConversionPolicy`)
5. Execute conversion if needed (`IBookConverter`)
6. Prepare managed storage — stage bytes (`IManagedStorage::PrepareImport`)
7. Process cover — decode/resize/re-encode (`ICoverImageProcessor`); skipped entirely when `ImportCovers=false`
8. Write DB entry (`IBookRepository::Add` via writer dispatcher)
9. Commit storage (`IManagedStorage::CommitImport`)
10. On failure at any step: rollback staged bytes (`IManagedStorage::RollbackImport`)

### Conversion Policy (`CImportConversionPolicy`)

| Source format | `force_epub_conversion` | Converter | Decision |
|---|---|---|---|
| EPUB | any | any | `StoreSource` (EPUB stored as-is) |
| FB2 | false | any | `StoreSource` (stored as `.fb2.gz`) |
| FB2 | true | available | `StoreConverted` (convert then store EPUB) |
| FB2 | true | unavailable | `FailImport` (hard failure, no fallback) |
| FB2 | true | cancelled | `CancelImport` (user-initiated, not a failure) |

### Cancellation Contract

- Main thread checks `stopToken.stop_requested()` before each new worker submission.
- Already-submitted workers run to completion; return `Cancelled` result if they detect the token.
- Phase 2 always collects all futures — no abandoned futures.
- `CLibraryImportFacade` detects `WasCancelled` after pipeline completes, collects imported IDs, invokes `CImportRollbackService::RollbackImportedBooks()`.
- Rollback is explicit; partial success visible to caller.

### Performance Telemetry (`CImportPerfTracker`)

Stages measured with cache-line-aligned atomic accumulators:

| Stage key | Measures |
|---|---|
| `parse` | XML/EPUB metadata parsing |
| `hash` | SHA-256 computation |
| `dedup` | `FindDuplicates` read call |
| `convert` | external converter process |
| `cover` | cover decode + resize + re-encode |
| `storage` | managed file staging |
| `db_wait` | time blocked on writer queue (`IBookRepository::Add`) |
| `zip_scan` | ZIP pre-scan (`CountPlannedEntries`) |
| `zip_extract` | main-thread extraction to temp |
| `sema_wait` | main thread blocked on back-pressure semaphore |

Periodic log: every 500 books or 30 seconds — throughput, per-stage averages, bottleneck %, writer queue depth.  
Outlier log: warn per book that takes > 5 seconds end-to-end.  
Summary log: total counts, elapsed time, throughput, bottleneck ranking.

---

## 7. Storage & Persistence Model

### Library Root Layout

```
<LibraryRoot>/
  Database/
    librova.db                         ← SQLite database (books, genres, tags, FTS5)
  Objects/
    <aa>/<bb>/<BookId>.book.<ext>      ← managed book bytes (sharded)
    <aa>/<bb>/<BookId>.cover.<ext>     ← extracted cover (JPEG)
  Logs/                                ← session logs (synced from native host)
  Trash/                               ← staging directory before Recycle Bin
```

### Object Path Sharding

BookId is zero-padded to 10 digits. `<aa>/<bb>` are the two lowest bytes of the FNV-1a hash of the padded ID (hex). Distribution is deterministic and even from the first import.

```
BookId = 42  →  padded = "0000000042"  →  hash lower 2 bytes  →  "Objects/3f/a7/0000000042.book.epub"
```

### Storage Encodings

| Format | Internal storage | Extension |
|---|---|---|
| EPUB | Plain bytes | `.book.epub` |
| FB2 (default) | gz-compressed | `.book.fb2.gz` (transparent to UI and export) |
| FB2 (forced conversion) | EPUB after conversion | `.book.epub` |

### Cover Processing Pipeline

1. Extract cover bytes from parsed book (`ICoverProcessor`).
2. Decode with `stbi_load_from_memory` (stb_image).
3. Compute target dimensions: primary `456×684`; fallback `400×600`.
4. JPEG re-encode: quality sequence `q85 → q78 → q72` until ≤ 120 KiB.
5. If still over budget after `q72`: store the result and emit a warning (no infinite loop).
6. All stored covers are JPEG regardless of source format.

### Duplicate Detection (Two-Layer)

| Layer | When | Mechanism | Outcome |
|---|---|---|---|
| Read-side | Before staging | `IBookQueryRepository::FindDuplicates(sha256_hex)` | Returns matches; coordinator decides reject or allow |
| Write-side | At `Add()` | `BEGIN IMMEDIATE` transaction re-checks `sha256_hex` | Throws `CDuplicateHashException(conflictingBookId)` on collision |

Empty `sha256_hex` skips the write-side check (treated as "unknown hash").  
`IBookRepository::ForceAdd()` bypasses the hash check entirely (explicit override).

### Database Schema (version 1)

| Table | Key columns | Notes |
|---|---|---|
| `books` | id, title, authors (JSON array), language, series, series_index, publisher, year, isbn, description, identifier, format, storage_encoding, managed_path, size_bytes, sha256_hex, cover_path, added_at_unix_ms | Primary book record |
| `genres` | id, normalized_name (unique, lowercase), display_name (English) | Genre lookup |
| `book_genres` | book_id, genre_id, source_type | `source_type`: `fb2_genre` or `epub_subject` — preserves provenance |
| `tags` | name (unique) | Freeform tag |
| `book_tags` | book_id, tag_id | Many-to-many |
| `search_index` | FTS5 virtual table: title, authors, tags, description, genres | Full-text search |

Schema version policy: `user_version 0` → create fresh DB + set to 1. `user_version 1` → no-op. Any other value → incompatibility error (manual deletion required).

---

## 8. Critical Invariants

Violating any of these causes data corruption, crashes, or silent test failures.

1. **`PathFromUtf8()` for all UTF-8 paths.** Never construct `std::filesystem::path` from `std::string`, `const char*`, or protobuf string fields directly — `path(string)` uses the ANSI codepage and silently corrupts Cyrillic. Always call `PathFromUtf8()` from `libs/Foundation/UnicodeConversion.hpp`.

2. **`inFlight` semaphore declared before `pool`.** In `CZipImportCoordinator` the back-pressure semaphore object must be declared before the thread pool in the same scope. The pool destructor calls `wait()` internally; worker lambdas reference `inFlight` by ref. If the semaphore is destroyed first, workers dangle.

3. **ZIP access is single-threaded.** `zip_t*` (libzip) is **not thread-safe**. All ZIP enumeration and file extraction must happen on the main thread. Workers only receive already-extracted paths.

4. **`AddBatch()` must return exactly one result per input.** `CWriterDispatchingRepository` validates this invariant; a size mismatch is treated as a fatal writer error and stops further submissions.

5. **`CloseSession()` before deleting the database file in tests.** Both `CSqliteBookRepository` and `CSqliteBookQueryRepository` hold open SQLite connections. Windows prevents deletion of files with open handles; tests that skip `CloseSession()` fail with a filesystem error.

6. **ID reservation before worker dispatch.** The main thread calls `IBookRepository::ReserveIds(count)` for the full batch before submitting any worker. Workers must not open SQLite transactions to obtain IDs; all concurrency is confined to the writer actor.

7. **Conversion cancellation ≠ converter failure.** `ImportConversionPolicy` distinguishes `CancelImport` (user cancelled mid-convert) from `FailImport` (converter binary missing or exited with error). Forced EPUB conversion with an unavailable converter is a hard failure, not a silent fallback to storing the original FB2.

8. **Per-entry working directories use ZIP index, not filename.** Subdirectories are `working_dir/entries/<zip_index>/` — never the entry's relative path. Identical filenames in different archive subdirectories would otherwise collide.

9. **DB schema version policy.** `CSchemaMigrator` accepts `user_version == 0` (creates fresh DB) or `user_version == 1` (no-op). Any other version throws an incompatibility error. Do not add automatic upgrade logic unless the change is non-destructive and the decision is explicit.

10. **Mandatory IPC boundary logging in both directions.** Every method in `CLibraryJobServiceAdapter` must log the key outcome (not only on failure). Every managed `*Service.cs` wrapping an IPC call must log the successful response. Error-only logs are a diagnostic gap.

11. **Futures are never abandoned.** In Phase 2 of `CZipImportCoordinator`, all `std::future` objects are resolved with `future.get()` even when cancellation was requested. Abandoning futures would detach worker threads from the pool and corrupt pool state.

12. **Rollback is explicit, not silent.** When import is cancelled after workers have committed books, `CLibraryImportFacade` collects all imported IDs and invokes `CImportRollbackService::RollbackImportedBooks()`. Partial success is always visible to the caller; it is never hidden.

---

## 9. Task Navigation

### Add a new RPC method
**Use the `$transport-rpc` skill** — it has the complete, ordered checklist for both sides (proto → C++ → C#, validation sequence, close-out).

Modules involved: `proto/import_jobs.proto`, `libs/Transport/` (`EPipeMethod`), `libs/Rpc/` (`CLibraryJobServiceAdapter`, `CImportJobProtoMapper`, `CLibraryCatalogProtoMapper`), `apps/Librova.UI/PipeTransport/`, and the relevant client/service/mapper in `ImportJobs/` or `LibraryCatalog/`.

### Change single-file import logic
- `libs/Import/SingleFileImportCoordinator.cpp/.hpp` — main orchestration
- `libs/Import/ImportConversionPolicy.cpp/.hpp` — conversion decision
- `libs/Import/ImportDiagnosticText.cpp` — user-facing error messages
- Tests: `tests/Unit/TestSingleFileImportCoordinator.cpp`

### Change parallel import (batch or ZIP)
- ZIP: `libs/Import/ZipImportCoordinator.cpp/.hpp`
- Batch loose files: `libs/App/LibraryImportFacade.cpp` (workload planner) + `libs/Import/ParallelImportHelpers.hpp`
- Writer: `libs/Import/WriterDispatchingRepository.cpp/.hpp`
- Performance tracking: `libs/Import/ImportPerfTracker.cpp/.hpp`
- Tests: `tests/Unit/TestZipImportCoordinator.cpp`, `tests/Unit/TestLibraryImportFacade.cpp`, `tests/Unit/TestWriterDispatchingRepository.cpp`

### Add a new book format
- Implement `IBookParser` in `libs/Parsing/` (new `<Format>Parser.hpp/.cpp` + register in `BookParserRegistry.cpp`)
- Extend `EBookFormat` in `libs/Domain/BookFormat.hpp` and update all switch statements
- Add proto enum value `BookFormat` in `proto/import_jobs.proto`; update `CLibraryCatalogProtoMapper`
- Add test in `tests/Unit/Test<Format>Parser.cpp`

### Change the database schema
**Use the `$sqlite` skill** for the current Librova-specific schema, query, FTS, and review checklist before editing the files below.

- SQL DDL is in `libs/Database/DatabaseSchema.cpp`
- Schema version enforcement is in `libs/Database/SchemaMigrator.cpp`
- If upgrading from version 1: add a migration branch, increment expected version, ensure it is non-destructive, document the decision
- Tests: `tests/Unit/TestSchemaMigrator.cpp`

### Add a field to the book card (parser → UI)
1. Add field to `SBookMetadata` in `libs/Domain/`
2. Parse it in `CFb2Parser` and/or `CEpubParser`
3. Persist it: add column in `libs/Database/DatabaseSchema.cpp`, update `CSqliteBookRepository` and `CSqliteBookQueryRepository`
4. Expose via `IBookQueryRepository` + `GetBookDetails` response
5. Add to proto `BookDetails` message
6. Map in `CLibraryCatalogProtoMapper`
7. Add to C# `BookDetailsModel` and `LibraryCatalogMapper`
8. Bind in `LibraryBrowserViewModel` / AXAML view

### Change cover processing
- Decode/resize/re-encode: `libs/Storage/StbCoverImageProcessor.cpp/.hpp`
- Size targets and quality fallback constants are inside that file
- Interface: `ICoverImageProcessor` in `libs/Domain/`
- Test: `tests/Unit/TestStbCoverImageProcessor.cpp`

### Change delete / Recycle Bin logic
- Facade: `libs/App/LibraryTrashFacade.cpp`
- Trash staging: `libs/Storage/ManagedTrashService.cpp`
- Recycle Bin: `libs/Storage/WindowsRecycleBinService.cpp`
- Proto: `MoveBookToTrashRequest/Response` and `DeleteDestination` enum in `proto/import_jobs.proto`
- Adapter: `CLibraryJobServiceAdapter::MoveBookToTrash()`

### Add a new ViewModel / View
- ViewModel: `apps/Librova.UI/ViewModels/<Name>ViewModel.cs` — derive from `ObservableObject`; commands use `AsyncCommand`
- View: `apps/Librova.UI/Views/<Name>View.axaml` + `.axaml.cs` code-behind
- Register in `ShellViewModel` or compose into parent ViewModel
- Styles: reuse design tokens from `apps/Librova.UI/Styles/` — see `docs/UiDesignSystem.md`

### Change converter settings
- Configuration and CLI command building: `libs/Converter/`
- External process execution: `libs/Converter/ExternalBookConverter.cpp`
- C# probe and validation: `apps/Librova.UI/Shell/Fb2ConverterProbe.cs`, `Shell/ConverterValidationCoordinator.cs`
- ViewModel: `ViewModels/ShellConverterPathController.cs`, `ShellConverterSettingsState.cs`

### Add a unit test (C++)
- File: `tests/Unit/Test<Area>.cpp`
- Framework: Catch2 (`TEST_CASE`, `REQUIRE`, `SECTION`)
- Temp database: call `repository.CloseSession()` in teardown before deleting the file
- Naming: describe observable behaviour (e.g., `"AddBatch returns exactly one result per request"`)

### Add a unit test (C#)
- File: `tests/Librova.UI.Tests/<Area>Tests.cs`
- Framework: xUnit (`[Fact]`, `[Theory]`)
- Null service stubs: use `NullLibraryCatalogService` or equivalent
- Naming: `Method_Scenario_ExpectedBehavior`

---

## 10. External Dependencies

| Library | Used in | Purpose |
|---|---|---|
| `pugixml` | `libs/Parsing/` | XML parsing for FB2 and EPUB OPF metadata |
| `libzip` | `libs/Import/` | ZIP archive enumeration and entry extraction |
| `sqlite3` (+ fts5 feature) | `libs/Database/` | Embedded relational database; FTS5 full-text search |
| `spdlog` | `libs/Foundation/`; all C++ modules | Structured, async-capable logging |
| `protobuf` | `libs/Rpc/` | Binary serialization of IPC messages |
| `abseil` | Pulled in transitively by protobuf | String utilities, hash maps |
| `stb` | `libs/Storage/` | Single-header image decode/encode (JPEG, PNG) for cover processing |
| `bshoshany-thread-pool` (BS::thread_pool) | `libs/Import/` | Fixed-size thread pool for parallel import workers |
| `zlib` | `libs/Storage/`; libzip dependency | gz compression/decompression for internal FB2 storage |
| `catch2` | `tests/Unit/` | C++ unit test framework |
| **Avalonia** | `apps/Librova.UI/` | Cross-platform XAML UI framework (.NET / C#) |
| **Google.Protobuf** | `apps/Librova.UI/`; generated C# proto bindings | C# protobuf serialization |
| **Serilog** | `apps/Librova.UI/Logging/` | Structured logging for C# UI process |

---

## 11. Code Conventions

> **Authoritative source**: `docs/CodeStyleGuidelines.md` and the `$code-style` skill. This section is a compact quick-reference only — if anything here conflicts with those sources, those sources win.

### C++ Naming Prefixes

| Prefix | Applies to | Example |
|---|---|---|
| `C` | Classes (concrete) | `CSingleFileImportCoordinator` |
| `I` | Interfaces (pure virtual) | `IBookRepository` |
| `S` | Structs (data / value types) | `SBookMetadata` |
| `E` | Enums | `EBookFormat` |
| `k` | Compile-time constants | `kMaxWorkerCount` |

**Other C++ rules (non-obvious):** Allman braces; member variables `m_memberName`; methods `PascalCase`; locals `camelCase`; file names `PascalCase`; one `.hpp`/`.cpp` pair per slice directory.

**C#:** types/methods `PascalCase`, private fields `_camelCase`, file-scoped namespaces, async suffix `Async`.

### Non-Negotiable Rules

- **English only** — code, comments, logs, UI strings, commit messages
- **`PathFromUtf8()`** — only approved construction of `std::filesystem::path` from UTF-8; `path(string)` is forbidden
- **One static library per logical slice** — each `libs/<Module>/` has its own `CMakeLists.txt`
- **IPC logging is mandatory** — adapter methods (C++) and service wrappers (C#) must log success outcomes, not only errors

### Proto Quick Rules

Field numbers: append-only, never reuse. Message names: `PascalCase`. Field names: `snake_case`. Enum values: `SCREAMING_SNAKE_CASE`. Enum zero: `UNKNOWN`/`UNSPECIFIED`.

### Log Level Guide

| Level | When to use |
|---|---|
| `Info` | Expected outcomes; normal state transitions; successful IPC responses |
| `Warn` | Unexpected-but-handled: `CanStartImport=false`, empty result, degraded operation |
| `Error` | Caught exceptions; failures surfaced to the user |
| `Critical` | Unrecoverable failures |
| `Debug` | High-frequency tracing (per-entry timings, duplicate checks) |

### Test Conventions

> Full policy: `AGENTS.md` § Verification and test discipline.

- C++ tests: Catch2 `TEST_CASE` / `REQUIRE` / `SECTION`
- C# tests: xUnit `[Fact]` / `[Theory]`; naming `Method_Scenario_ExpectedBehavior`
- Mandatory test areas: Unicode + Cyrillic, duplicate detection, rollback after cancellation, forced conversion failures
- **DB teardown**: call `repository.CloseSession()` before deleting temp DB file (Windows file handle)
- No fixed sleeps in IPC tests — explicit readiness signals and deterministic cleanup

---
## 12. Domain Interfaces Reference

Quick reference for core abstractions in `libs/Domain/`. All implemented in higher layers; consumed by import pipeline components.

### `IBookRepository` (write)

```cpp
SBookId    ReserveId();
std::vector<SBookId> ReserveIds(size_t count);
SBookId    Add(const SBook& book);          // throws CDuplicateHashException on sha256 collision
SBookId    ForceAdd(const SBook& book);     // bypass hash check
std::optional<SBook> GetById(SBookId id) const;
void       Remove(SBookId id);
void       Compact(const std::function<void()>& onProgressTick = nullptr);
void       OptimizeSearchIndex();
void       RemoveBatch(std::span<const SBookId> ids);
std::vector<SBatchBookResult> AddBatch(std::span<const SBatchBookEntry> entries);
```

### `IBookQueryRepository` (read)

```cpp
std::vector<SBook>       Search(const SSearchQuery& query) const;
std::uint64_t            CountSearchResults(const SSearchQuery& query) const;
std::vector<SFacetItem>  ListAvailableLanguages(const SSearchQuery& query) const;
std::vector<std::string> ListAvailableTags(const SSearchQuery& query) const;
std::vector<SFacetItem>  ListAvailableGenres(const SSearchQuery& query) const;
std::vector<SDuplicateMatch> FindDuplicates(const SCandidateBook& candidate) const;
SLibraryStatistics       GetLibraryStatistics() const;
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
void             CommitImport(const SPreparedStorage& prepared);
void             RollbackImport(const SPreparedStorage& prepared) noexcept;
```

### `IBookParser`

```cpp
bool        CanParse(EBookFormat format) const;
SParsedBook Parse(
    const std::filesystem::path& path,
    std::string_view logicalSourceLabel,
    const SBookParseOptions& options) const;
```

### `IBookConverter`

```cpp
bool              CanConvert(EBookFormat source, EBookFormat dest) const;
SConversionResult Convert(const SConversionRequest& request,
                          IProgressSink& progressSink,
                          std::stop_token stopToken) const;
```

### Key Domain Value Types

| Type | Key fields |
|---|---|
| `SBook` | `Id`, `Metadata`, `File`, `CoverPath`, `AddedAtUtc` |
| `SBookMetadata` | `TitleUtf8`, `AuthorsUtf8[]`, `Language`, `SeriesUtf8`, `SeriesIndex`, `PublisherUtf8`, `Year`, `Isbn`, `TagsUtf8[]`, `GenresUtf8[]`, `DescriptionUtf8`, `Identifier` |
| `SBookFileInfo` | `Format` (`EBookFormat`), `StorageEncoding` (`EStorageEncoding`), `ManagedPath`, `SizeBytes`, `Sha256Hex` |
| `SParsedBook` | `Metadata`, `SourceFormat`, `CoverExtension`, `CoverBytes`, `CoverDiagnosticMessage` |
| `SStoragePlan` | `BookId`, `Format`, `StorageEncoding`, `SourcePath`, optional `CoverSourcePath` |
| `SPreparedStorage` | `StagedBookPath`, `StagedCoverPath`, `FinalBookPath`, `FinalCoverPath`, `RelativeBookPath`, `RelativeCoverPath` |

---

## 13. Process Startup & Shutdown

### Native Host (`apps/Librova.Core.Host/Main.cpp`)

1. Parse CLI: library root, working directory, parent UI PID, pipe name.
2. Initialize spdlog (log to `LibraryRoot/Logs/`).
3. Run `CSchemaMigrator` — create DB (version 0 → 1) or validate (version 1 = no-op); error on any other version.
4. Construct services: `CSqliteBookRepository`, `CSqliteBookQueryRepository`, `CManagedFileStorage`, parsers, converter, facades, job manager.
5. Start `CNamedPipeHost` — begin listening for requests.
6. Bind lifetime to parent process identity: monitor the UI process using `pid + creation time`; self-terminate if that exact process dies.
7. Dispatch loop: `EPipeMethod` → `CLibraryJobServiceAdapter` → proto response.
8. Shutdown: stop accepting; drain in-flight jobs; flush logs.

### C# UI (`apps/Librova.UI/Program.cs` → `ShellBootstrap`)

1. Load `ShellStateStore` (window geometry, last library path) + `UiPreferencesStore` (converter path).
2. If no library → show `FirstRunSetupViewModel` wizard.
3. Validate library root (`LibraryRootValidation`): requires `Database/librova.db`.
4. Probe FB2 converter (`Fb2ConverterProbe`): set `UiConverterMode`.
5. Spawn native host (`CoreHostProcess`): pass library root, pipe name, own PID, and own process creation time.
6. Create IPC services: `ImportJobsService`, `LibraryCatalogService`.
7. Construct `ShellApplication` → `ShellViewModel` → show `MainWindow`.
8. On exit: send graceful shutdown request to host; wait with timeout; force-terminate if no response.

---

*See also: `docs/CodeStyleGuidelines.md` (full style reference), `docs/UiDesignSystem.md` (UI design system).*

---

## 14. Architecture Decisions

Frozen design decisions for Librova. When a structural question is not answered by Hard Constraints in `AGENTS.md`, read this section. Do not change decisions here without an explicit architecture discussion.

### 14.1 Runtime Model

Librova is a two-process desktop application.

**`Librova.UI` (C# / .NET / Avalonia) owns:**
- windowing and user interactions
- desktop dialogs
- ViewModels and UI-facing services

**`Librova.Core` (C++20) owns:**
- domain logic
- import pipeline
- parsing and format handling
- conversion orchestration
- persistence (SQLite + FTS5)
- managed storage
- async job lifecycle
- search
- file export and staged delete behavior (Windows Recycle Bin)

These boundaries are frozen. Do not collapse into one process; do not move domain logic into the UI layer.

### 14.2 IPC Boundary

The transport is **Protobuf contracts over Windows named pipes**. This is the canonical runtime boundary between UI and core. There is no gRPC runtime and no P/Invoke as the primary transport — both are explicitly excluded.

**Process lifetime safety:** the UI passes its own PID and process creation time to the native host at startup; the host binds its lifetime to that exact UI process instance and self-terminates if it dies or is force-killed.

**Normal shutdown:** the UI must request a graceful host stop first, then fall back to forced termination only if the graceful request times out.

### 14.3 Storage Design

**Write-session lifecycle:** `CSqliteBookRepository` keeps a single SQLite write connection open for the lifetime of the repository, protected by a mutex-serialised write session. In tests, call `repository.CloseSession()` before deleting the database file — Windows does not allow deleting a file with an open handle.

**Import safety principles:**
- Avoid partial visible success where possible.
- Clean stale runtime workspace state on startup.
- Keep rollback and failure semantics explicit — never hide partial success from callers.

**Managed library invariants:**
- Hot runtime workspace (import workspaces, converter working dirs, staging) is intentionally separate from the managed library root. These are synchronized back only where needed.
- Managed FB2 files are stored compressed as `.book.fb2.gz` under `Objects/` as an internal storage detail; browse, export, delete, and duplicate behavior treat them as ordinary FB2 books.
- `Trash/` provides rollback-safe staging for delete operations before Recycle Bin handoff. If the Windows handoff fails, the delete remains committed in the catalog and staged files stay in `Trash/` as an explicit managed fallback.

**Library-root bootstrap modes:**
- `Create Library` — may initialize only a new or empty target directory.
- `Open Library` — must validate that the selected root is already a complete managed library, including `Database/librova.db`.
- Startup recovery for a damaged library — must not silently recreate the library in place; must accept an alternative path.

**Database schema version policy:** `CSchemaMigrator` accepts `user_version == 0` (fresh DB, creates schema at version 1) or `user_version == 1` (no-op). Any other version throws an incompatibility error requiring the user to delete the library database. No automatic upgrade paths are provided unless the change is non-destructive and the decision is deliberate. When upgrading is appropriate, use the dispatch pattern:
```cpp
if (currentVersion < 2) { UpgradeToVersion2(connection); }
if (currentVersion < 3) { UpgradeToVersion3(connection); }
```
Bump `GetCurrentVersion()` in `DatabaseSchema.cpp` to match.

### 14.4 Import and Conversion Rules

| Source format | `force_epub_conversion` | Converter | Decision |
|---|---|---|---|
| EPUB | any | any | Store as EPUB (as-is) |
| FB2 | false | any | Store as `.book.fb2.gz` (managed FB2) |
| FB2 | true | available | Convert → store as EPUB |
| FB2 | true | unavailable | Hard failure — do not fall back to storing the original FB2 |
| FB2 | true | cancelled | Cancellation — distinct from converter failure |

**Additional rules:**
- FB2 metadata parsing must preserve non-UTF-8 legacy encodings that still appear in real libraries, including Windows-1251.
- Duplicate detection rejects by default; the UI may explicitly override and store the new item as a separate managed record via `IBookRepository::ForceAdd`.
- Conversion cancellation is never treated as ordinary converter failure; the import entry does not silently downgrade to storing the original source.
