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

Series and genres are supported coherently through parser output, persistence, transport contracts, and UI-facing filters/details.

## 5.1 Genre Model

Genres are a first-class entity, separate from user-defined tags:

- **`genres` table**: stores deduplicated genres with `normalized_name` (lowercase lookup key) and `display_name` (human-readable, stored in English, e.g. `"Science Fiction"`).
- **`book_genres` table**: join table linking books to genres, with a mandatory `source_type` provenance column. Valid values:
  - `fb2_genre` — parsed from an FB2 `<genre>` element; FB2 raw codes are resolved to human-readable display names via `CFb2GenreMapper` at parse time.
  - `epub_subject` — parsed from an EPUB `dc:subject` element; stored as-is without FB2 vocabulary transformation.

This model guarantees that FB2 genre codes like `sf` are never stored as bare raw codes in the database, and that EPUB subjects are never corrupted by FB2-specific normalization.

**Known display-name aliases**: `adv_history` (Adventure › History) and `sci_history` (Science › History) both map to the display name `"History"`. Books carrying one of these codes will appear under a single "History" filter entry. Distinguishing the two categories at the filter level is deferred post-MVP.

**Domain**: `SBookMetadata::GenresUtf8` carries parsed genres. `SBookMetadata::TagsUtf8` remains for future user-defined tags and is currently unused by parsers.

**FTS**: the `search_index` virtual table includes a `genres` column alongside `title`, `authors`, `tags`, and `description`.

## 5.2 Database Schema Version Policy

The library SQLite database carries a `user_version` PRAGMA that Librova checks at startup.

**Current schema version: 1.**

`CSchemaMigrator::Migrate` enforces the following rules:

- **`user_version == 0`** — brand-new database; applies the full schema script and sets `user_version = 1`.
- **`user_version == current`** — already up to date; no-op.
- **any other value** — version is incompatible; throws an error requiring the user to delete the library database so Librova can recreate it.

**No automatic upgrade paths are provided for existing databases.** This is an intentional policy: the schema was simplified in version 1 (genres/tags separated, migration infrastructure removed) and compatibility with pre-v1 databases cannot be provided without re-introducing that complexity.

**Future migrations**: when a future schema change can be handled non-destructively (e.g., adding a nullable column or a new table), restore the upgrade dispatch pattern in `CSchemaMigrator::Migrate`:

```cpp
if (currentVersion < 2) { UpgradeToVersion2(connection); }
if (currentVersion < 3) { UpgradeToVersion3(connection); }
```

Bump `GetCurrentVersion()` in `DatabaseSchema.cpp` to match. If the change is breaking, keep the incompatibility error and require DB recreation instead.

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

- [Librova Backlog](backlog.yaml)

## 9. Parallel Import Pipeline

This section is the authoritative specification for all concurrency decisions made in the import subsystem. Any future change to import threading **must** be consistent with the invariants stated here.

### 9.1 Motivation and Scope

The sequential import baseline processes approximately 2.5 books/second (47 911 books ≈ 5 h 19 min at that rate). Per-book work — SHA-256 hashing, XML parsing, cover decoding, file staging, SQLite write — is fully independent across books and dominates wall-clock time. Parallelising this work is the primary lever.

Parallelism is applied in two places:

- inside **ZIP imports** (`CZipImportCoordinator`) for entry-level parallelism;
- inside **batch-mode loose-file imports** (`CLibraryImportFacade::Run()`) for contiguous non-ZIP stretches, while preserving the original mixed-source order seen by the caller.

Single-file imports still remain sequential; single-ZIP imports rely on the ZIP coordinator's internal worker pool.

### 9.2 Thread Count Formula

```cpp
const auto hwThreads   = std::thread::hardware_concurrency();
const auto threadCount = static_cast<std::size_t>(
    std::max(1u, std::min(8u, hwThreads > 1u ? hwThreads - 1u : 1u)));
```

Rules:

- Always at least **1** worker.
- At most **8** workers — avoids I/O saturation on typical consumer SSDs.
- On multi-core machines, leaves **1 hardware thread free** for the OS and the Avalonia UI process.
- On a single-core machine (`hardware_concurrency() == 1`) still uses 1 worker, so the pipeline does not dead-lock waiting on itself.

### 9.3 Two-Phase ZIP Import

`CZipImportCoordinator::Run()` operates in two clearly separated phases executed on the same calling (main import job) thread:

#### Phase 1 — Extract and Submit

The main thread iterates ZIP entries sequentially. `zip_t*` (libzip) is **not thread-safe**; all archive access remains on the main thread throughout the import.

For each entry:

1. **Immediate skip** (no I/O): directory entries, nested `.zip` archives, unsafe relative paths, unsupported formats — produce an `SZipEntryImportResult` immediately and are stored in the ordered slot vector without touching the semaphore or thread pool.
2. **Cancellation check**: if `stopToken.stop_requested()` or `progressSink.IsCancellationRequested()`, Phase 1 exits the loop. No new tasks are submitted; already-submitted tasks run to completion.
3. **Semaphore acquire**: `inFlight.acquire()` — blocks the main thread if too many files are already on disk. This is the **back-pressure mechanism** that bounds peak temp space.
4. **Extraction**: `ExtractEntryToWorkspace()` writes the entry bytes to `workingDirectory/extracted/<relative-path>`. On failure, releases the semaphore immediately, stores a `Failed` slot, and continues.
5. **Submit to pool**: `pool.submit_task(lambda)` — returns `std::future<SZipEntryImportResult>`. The future is stored in the ordered slot vector.

#### Phase 2 — Finalise in Order

Worker completion is tracked through a main-thread-owned progress queue. Whenever any worker finishes, it pushes a small completion event; the main thread drains that queue between submissions and while waiting for remaining workers, updating counters and calling `IStructuredImportProgressSink::ReportStructuredProgress` immediately. This keeps progress incremental without letting workers touch the outer sink directly.

After all submitted workers finish, the main thread iterates `slots` in original ZIP-entry order:

- Immediate slots are moved out directly.
- Future slots are resolved via `future.get()` (now non-blocking because all workers have already completed).

Result ordering therefore remains deterministic even though progress is completion-driven rather than ready-prefix-driven.

### 9.4 Ordered Slot Vector

Result ordering is preserved by the `SEntrySlot` structure:

```cpp
struct SEntrySlot {
    std::optional<SZipEntryImportResult>              Immediate;
    std::optional<std::future<SZipEntryImportResult>> Future;
};
std::vector<SEntrySlot> slots;
```

Exactly one of the two `optional` members is populated per slot. Final materialisation reads slots in insertion order, which matches the original ZIP index order. This invariant must be maintained: **never** move immediate or future results into separate collections that are later merged.

### 9.5 Semaphore and Disk Usage Bound

```cpp
// inFlight MUST be declared BEFORE pool in Run() so it outlives pool's destructor.
std::counting_semaphore<64> inFlight(static_cast<std::ptrdiff_t>(threadCount) * 2);
BS::thread_pool<> pool(threadCount);
```

The semaphore has capacity `threadCount × 2`. The main thread acquires one slot before each extraction. The worker thread releases its slot at the end of processing via RAII:

```cpp
struct SSemGuard {
    std::counting_semaphore<64>& sem;
    ~SSemGuard() noexcept { sem.release(); }
} semGuard{inFlight};
```

**Consequence**: at most `threadCount × 2` extracted files exist on disk simultaneously. With `threadCount = 8` and typical FB2 files of ≈500 KB, peak temp usage is **≈8–16 MB** rather than gigabytes.

**Declaration order is a hard invariant.** `inFlight` must be declared before `pool` in every scope that uses both. The `pool` destructor calls `wait()` internally, which blocks until all worker lambdas return. Those lambdas reference `inFlight` by pointer/reference. If `inFlight` were destroyed first, workers would use a dangling reference.

### 9.6 Worker Entry Point

Each worker lambda captures:

| Captured value | Type | Reason |
|---|---|---|
| `this` | const pointer | access to `m_singleFileImporter` |
| `entryPath` | by value | for logging (ZIP-relative path) |
| `extractedPath` | by value (moved) | unique per-entry extracted file |
| `entryWorkDir` | by value (moved) | unique per-entry working directory |
| `request` | by reference (`const`) | immutable import parameters |
| `inFlight` | by reference | RAII semaphore release |
| `perf` | by reference | thread-safe atomic accumulators |
| `stopToken` | by value | thread-safe read-only token |

Worker steps:

1. Install `SSemGuard` for guaranteed semaphore release.
2. Create `CNullProgressSink` (workers do not report sub-entry progress; only the main thread reports aggregate progress after draining completion events).
3. Call `m_singleFileImporter.Run(...)` with the per-entry `WorkingDirectory` and `extractedPath` as source.
4. `CleanupExtractedEntryNoThrow` — delete extracted file.
5. `CleanupEntryWorkspaceNoThrow` — delete per-entry working directory.
6. `perf.NoteOutlierIfSlow(...)` — emit `Warn` if total elapsed > 5 s.
7. `perf.OnBookProcessed(...)` — update atomic counters; may emit periodic log.
8. Return `SZipEntryImportResult`.

On exception from `singleFileImporter.Run()`, cleanup still happens in the catch block and the worker returns `Failed`.

### 9.7 Per-Entry Working Directory

Workers must not share any filesystem path. The working directory for entry index `i` (ZIP entry index, 0-based) is:

```
workingDirectory / "entries" / "<i>"
```

The index is the raw `zip_uint64_t` ZIP entry index, not derived from the filename stem. This guarantees uniqueness even when the archive contains identically named files in different subdirectories.

**Forbidden**: stem-based paths like `entries/first` — they collide when entries from different subdirectories share the same filename base.

### 9.8 Thread Safety of Import Components

| Component | Thread-safe | Rationale |
|---|---|---|
| `CSingleFileImportCoordinator` | ✅ | All member state is const references; no mutable shared state |
| `CBookParserRegistry` | ✅ | Stateless XML parsing |
| `CManagedFileStorage` | ✅ | Per-`BookId` paths; `create_directories` is idempotent |
| `CStbCoverImageProcessor` | ✅ | `stbi_failure_reason()` uses `__declspec(thread)` on MSVC — thread-local |
| `CSqliteBookRepository::Add()` | ✅ | `BEGIN IMMEDIATE` serialises concurrent writes at the SQLite level |
| `CSqliteBookRepository::FindDuplicates()` | ✅ | Read-only; SQLite serialized mode |
| `CExternalBookConverter::Convert()` | ✅ | Isolation guaranteed by per-entry `WorkingDirectory` |
| `CZipArchive` / `zip_t*` | ❌ | **Not thread-safe.** All extraction stays on the main thread. |
| `IProgressSink` (outer) | ❌ | Not thread-safe. Workers use `CNullProgressSink`; only the main thread drains completion events and reports aggregate progress. |
| `CImportPerfTracker` | ✅ | All accumulators are `std::atomic`; aligned to 64-byte cache lines to prevent false sharing |

### 9.9 Writer Actor: `CWriterDispatchingRepository`

`CWriterDispatchingRepository` wraps `IBookRepository` and routes all `Add()` and `ForceAdd()` calls through a **single dedicated writer thread**, batching them into multi-row `AddBatch()` transactions.

**Purpose**: reduces the per-book SQLite overhead from one `BEGIN IMMEDIATE … COMMIT` per book to one transaction per batch of up to 50 books. Workers are never held waiting on each other's write — they wait only on their own future, which resolves as soon as the writer thread processes their entry within a batch.

Parallel callers reserve the needed `BookId` values **up front on the main thread** before they submit worker tasks. Workers therefore never open a SQLite transaction just to obtain an id; concurrent SQLite write activity remains confined to the writer actor.

**Lifecycle**:

```
Constructor → starts writer thread (m_writerThread).
Workers call Add()/ForceAdd() → Submit() → push SWriteRequest onto queue → notify → return future.
Worker calls future.get() → blocks until promise is resolved by writer thread.
After pool.wait() → caller must call Drain().
Drain() → sets m_stopped = true → notifies writer thread → joins.
Destructor calls Drain() if not already called.
```

**Writer loop invariant**: the writer thread drains up to `kMaxBatchSize` (50) requests from the queue per iteration. If fewer than 50 are available, it waits up to 10 ms before draining whatever arrived. If the queue is empty and `m_stopped` is set, the thread returns.

**AddBatch exception isolation**: if `AddBatch()` itself throws (e.g., a fatal SQLite error), all promises in that batch are resolved with `Failed` status so no worker future is left hanging. The error is logged at `Error` level.

**AddBatch result-size invariant**: `AddBatch()` must return exactly one `SBatchBookResult` per queued request. Any size mismatch is treated as a fatal writer error and stops further writes rather than letting callers observe undefined data.

**CDuplicateHashException rethrow**: when `Add()` receives a result with `Status = RejectedDuplicate`, it throws `CDuplicateHashException{result.ConflictingBookId.value_or(SBookId{})}`. This preserves the existing duplicate-rejection contract for callers of `IBookRepository::Add()`.

**Read and management operations** (`ReserveId`, `ReserveIds`, `GetById`, `Remove`, `Compact`) are forwarded directly to the underlying repository without queuing.

`CWriterDispatchingRepository` is now injected as the repository override for both ZIP-entry workers and batch-mode loose-file workers. `CLibraryImportFacade` owns one writer actor per import run and drains it explicitly before cancellation rollback or returning to the caller.

### 9.10 Performance Telemetry: `CImportPerfTracker`

`CImportPerfTracker` measures per-stage cumulative time across all workers using `std::atomic` accumulators aligned to 64-byte cache lines (one `SStageStats` per stage).

**Measured stages**:

| Stage key | What it covers |
|---|---|
| `parse` | XML/EPUB metadata parsing |
| `hash` | SHA-256 computation (BCrypt) |
| `dedup` | `FindDuplicates` read call |
| `convert` | FB2→EPUB external converter |
| `cover` | Cover image decode and resize (stb) |
| `storage` | Managed file staging |
| `db_wait` | Time blocked on `IBookRepository::Add()` (includes SQLite lock contention) |
| `zip_scan` | ZIP pre-scan used to count planned entries |
| `zip_extract` | Main-thread ZIP extraction into the temp workspace |
| `sema_wait` | Time the **main thread** was blocked on `inFlight.acquire()` — measures extraction back-pressure |

Managed cover normalization is canonicalized during import rather than preserving source JPEG bytes verbatim. The current policy re-encodes every extracted cover to JPEG, keeps the primary bound at `456x684` (3x the library-card cover slot), targets `<= 120 KiB` per stored cover, and falls back through JPEG quality `85 -> 78 -> 72` before a secondary `400x600` pass with the same lower-quality tail. The final `400x600 @ q72` step is a deterministic terminal fallback: if a pathological image still exceeds the byte budget, Librova stores that final result and logs the over-budget outcome instead of looping or preserving the original oversized bytes.

`sema_wait` is excluded from bottleneck percentages because it is the main thread's coordination overhead, not worker CPU time.

`CLibraryImportFacade` creates one run-level tracker and passes it through the entire import run, including ZIP sources. When a ZIP import is executed standalone without an outer tracker, `CZipImportCoordinator` creates its own local tracker and still emits a summary.

The tracker counts every terminal supported-book outcome that contributes to throughput: imported, duplicate-rejected, cancelled worker result, extraction failure, and worker exception. Unsupported and nested-archive skips are not counted as processed books in telemetry because they never enter the book-processing stages.

**Periodic log** (`[import-perf]`): emitted at most once per 500 books OR once per 30 seconds — whichever fires first. Uses a `compare_exchange_strong` race-free claim so only one thread emits per interval. Contains:
- total book count, throughput (books/sec)
- per-stage average latency (ms)
- bottleneck percentage (sorted by total time)
- writer queue depth sample from `CWriterDispatchingRepository`
- imported / duplicate / failed counters

**Outlier log**: a single `Warn` per slow book (> 5 000 ms end-to-end wall time per entry).

**Writer queue depth warning**: a single `Warn` (once per import run) when the sampled queue depth reaches 16. This matches the current in-flight back-pressure cap (`threadCount * 2`, max 16), so the warning fires when the writer queue actually saturates under the MVP worker limits.

**Summary log** (`[import-perf] SUMMARY`): called once after all workers complete (`LogSummary()`). Contains total counts, elapsed time in `Xm Ys`, throughput in books/sec, and a bottleneck ranking sorted by cumulative stage time.

### 9.11 Cancellation Contract

`std::stop_token` is thread-safe for concurrent read (`stop_requested()`). Workers check the token inside `ISingleFileImporter::Run()`. The cancellation contract is:

1. The main thread checks the token and `progressSink.IsCancellationRequested()` before submitting each new entry (Phase 1 loop head).
2. If cancellation is signalled mid-Phase-1, the loop breaks. No new tasks are submitted.
3. Already-submitted tasks run to completion, returning `Cancelled` status if they detect the token internally.
4. Phase 2 collects all futures unconditionally — no future is abandoned.
5. `CLibraryImportFacade::Run()` checks `WasCancelled` after the pipeline completes and initiates rollback of all `ImportedBookIds` collected in that run.

For batch-mode loose-file imports, a worker result of `Cancelled` contributes to the visible skipped count (matching ZIP aggregate progress), while source files that were never submitted after cancellation remain unprocessed and do not artificially advance the processed counter to `TotalEntries`.

**No orphan temp files on cancellation**: worker lambdas always call `CleanupExtractedEntryNoThrow` and `CleanupEntryWorkspaceNoThrow` regardless of result status, including the cancellation path.

### 9.12 What Is NOT Parallel

The following remain sequential by design:

| Operation | Reason |
|---|---|
| ZIP file enumeration and extraction | `zip_t*` is not thread-safe |
| `CountPlannedEntries()` pre-scan | Separate sequential ZIP pass before Phase 1 |
| Final ordered result materialisation | Single-threaded so caller-visible ZIP entry order stays deterministic |
| Single-file import mode (`EImportMode::SingleFile`) | One file — no parallelism benefit |
| Single-ZIP import mode (`EImportMode::ZipArchive`) | `CZipImportCoordinator` is already parallel internally |
| `CImportJobManager` outer job thread | One job = one `std::jthread`; concurrency is within the job |
| FB2→EPUB converter (batch mode) | Individual `CExternalBookConverter::Convert()` calls per worker; batch converter API deferred to backlog |

Batch mode single-file candidates (`EImportMode::Batch`, non-ZIP) **are now parallelised** in `CLibraryImportFacade::Run()` using the same BS::thread_pool plus main-thread completion-queue pattern as `CZipImportCoordinator`. The facade processes contiguous non-ZIP stretches so ZIP and loose-file sources keep their original caller-visible order, while `"Processed source file"` progress snapshots advance as soon as any worker finishes.
