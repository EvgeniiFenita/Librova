# LibriFlow Project Documentation

This document records the stable, already-implemented facts of the repository.

Unlike the architecture roadmap, this file should describe what is already true in the project today.

Update it when an implementation detail becomes stable enough to be treated as current project reality.

## 1. Product Baseline

- LibriFlow is a Windows-first desktop application for managing a personal e-book library.
- The MVP supports `EPUB`, `FB2`, and `ZIP` import.
- The UI is planned as `C# / Avalonia`.
- `apps/LibriFlow.UI` now also contains the first non-visual UI infrastructure for resolving and launching the native core host process during development.
- `apps/LibriFlow.UI` now also generates C# protobuf contracts from `proto/import_jobs.proto` and contains the first managed implementation of the named-pipe framing protocol.
- `apps/LibriFlow.UI` now also contains a real managed named-pipe RPC client and an import-job client over generated protobuf contracts.
- `apps/LibriFlow.UI` now also contains a UI-facing import-jobs service and DTO layer that hides generated protobuf types from future ViewModels.
- `apps/LibriFlow.UI` now also contains the first shell bootstrap/session layer that owns native host lifetime and exposes UI-facing services.
- `apps/LibriFlow.UI` now also contains the first ViewModel-ready shell and import-jobs state layer with observable state and async command support.
- `tests/LibriFlow.UI.Tests` now provides the first C# test baseline over UI-side core-host launch infrastructure.
- The core is implemented in `C++20`.
- The system architecture is two-process and uses `Protobuf` contracts at the process boundary.
- The current MVP transport direction is `Protobuf over Windows named pipes`, not mandatory `gRPC`.

## 2. Repository Baseline

- Native code is organized as one static library per logical slice under `libs/<SliceName>/`.
- UI applications live under `apps/`.
- The repository now also contains the first native host executable under `apps/LibriFlow.Core.Host`.
- Repository-level documentation lives under `docs/`.
- Shared protobuf contracts live under `proto/`.
- Build artifacts are routed under the repository root `out/`.
- `CMake` is the canonical native build system.

## 3. Application Layer

- A first application-facing import facade is implemented.
- The facade dispatches between:
  - single-file import
  - ZIP archive import
- The facade returns one aggregated result shape for higher layers instead of exposing low-level coordinator types directly.

## 4. Implemented Native Slices

Implemented slices at this point:

- `Application`
- `ApplicationClient`
- `ApplicationJobs`
- `Jobs`
- `Domain`
- `Core`
- `CoreHost`
- `DatabaseSchema`
- `DatabaseRuntime`
- `Sqlite`
- `BookDatabase`
- `StoragePlanning`
- `ManagedStorage`
- `ZipImporting`
- `EpubParsing`
- `Fb2Parsing`
- `ParserRegistry`
- `PipeClient`
- `PipeHost`
- `PipeTransport`
- `ProtoContracts`
- `ProtoMapping`
- `ProtoServices`
- `SearchIndex`
- `ConverterCommand`
- `ConverterConfiguration`
- `ConverterRuntime`
- `ImportConversion`
- `Importing`
- `Logging`

## 4.1 Shared Contracts

- The repository now contains the first shared protobuf contract file in [proto/import_jobs.proto](C:\Users\evgen\Desktop\LibriFlow\proto\import_jobs.proto).
- Protobuf package baseline is `libriflow.v1`.
- `.proto` file naming is now fixed as `snake_case`.
- The first contract covers:
  - import request DTO
  - import summary DTO
  - import job snapshot DTO
  - import job result DTO
  - `LibraryJobService` RPC surface for start/get/wait/cancel/remove
- Current protobuf contracts are transport-oriented and do not expose internal storage layout or low-level duplicate internals.
- `protobuf` is now part of the repository `vcpkg` manifest, and `protoc` is available from the project-local manifest toolchain under `out/build/<preset>/vcpkg_installed/x64-windows/tools/protobuf/protoc.exe`.
- The repository contains a PowerShell helper for schema validation: `scripts/ValidateProto.ps1`.
- `import_jobs.proto` is now compiled into a native C++ target through `libs/ProtoContracts`.
- The repository already has round-trip protobuf tests against generated C++ message classes.
- `libs/ProtoMapping` converts between:
  - `Application::SImportRequest` and `ImportRequest`
  - `ApplicationJobs::SImportJobSnapshot` and `ImportJobSnapshot`
  - `ApplicationJobs::SImportJobResult` and `ImportJobResult`
  - domain errors and transport `ErrorCode`
- Path mapping in the protobuf transport layer uses UTF-8 conversion instead of locale-dependent narrow path handling.
- `libs/ProtoServices` provides the first service adapter over protobuf messages:
  - `StartImport`
  - `GetImportJobSnapshot`
  - `GetImportJobResult`
  - `WaitImportJob`
  - `CancelImportJob`
  - `RemoveImportJob`
- The current adapter is transport-neutral and uses protobuf request/response types directly, but it does not yet depend on a real gRPC server runtime.
- The repository intentionally does not require a `gRPC C++` runtime for the MVP transport path.
- `libs/PipeTransport` now provides the first transport foundation for `Protobuf over named pipes`:
  - binary request/response envelope framing
  - explicit pipe method ids for `LibraryJobService`
  - protobuf request dispatch into the transport-neutral service adapter
  - structured transport statuses for invalid request, unknown method, and internal error
  - synchronous Win32 named-pipe channel wrappers with length-prefixed message exchange
  - real round-trip coverage against a live Windows named pipe
- `libs/PipeHost` now provides the first core-side named-pipe host loop:
  - accepts one connected pipe session
  - reads one framed protobuf request
  - dispatches it through `PipeRequestDispatcher`
  - writes one framed protobuf response back to the client
- `libs/PipeClient` now provides the first typed client helper for named-pipe RPC:
  - opens a pipe connection on demand
  - serializes protobuf requests
  - validates request/response correlation through request ids
  - translates invalid framing or non-`Ok` transport status into transport exceptions
  - parses typed protobuf responses for callers
- `libs/ApplicationClient` now provides an application-facing import job client above the raw pipe helper:
  - `Start`
  - `TryGetSnapshot`
  - `TryGetResult`
  - `Cancel`
  - `Wait`
  - `Remove`
- The application-facing pipe client maps protobuf DTOs back into repository application DTOs, so future UI code does not need to work with generated protobuf types directly.
- `libs/CoreHost` now provides command-line parsing for the native host process:
  - required `--pipe`
  - required `--library-root`
  - optional `--serve-one`
  - optional `--max-sessions`
  - optional built-in `fb2cng` converter settings
  - optional custom converter command settings
- `apps/LibriFlow.Core.Host` now builds the first real native host executable:
  - ensures managed library directories exist
  - clears stale `Temp/` state during startup recovery
  - migrates SQLite schema on startup
  - composes parser registry, repositories, managed storage, import facade, job service, protobuf adapter, and named-pipe host
  - serves sequential named-pipe sessions according to host options
  - treats malformed or abruptly closed pipe sessions as per-session errors instead of terminating the whole host process
- the current pipe client timeout is applied as an RPC deadline for waiting on the response after connect, not only as an initial connect timeout
- `libs/Logging` now provides the first native logging facade over `spdlog`
- the current core logging baseline writes host logs into `Logs/host.log` and mirrors the same records to `stderr`

## 5. Persistence And Storage

- SQLite is the active local database technology.
- The schema includes `books`, `authors`, `book_authors`, `tags`, `book_tags`, `formats`, and `search_index`.
- SQLite connections enable `foreign_keys` per connection.
- Book ids can be reserved before persistence so managed storage planning can use stable `BookId`-based paths before the final repository insert.
- Reserved book ids are allocated through a dedicated SQLite sequence row instead of `MAX(id) + 1`, so sequential reservations from different repository instances do not collide.
- Managed storage uses stable `BookId`-based paths under `Books/`, `Covers/`, and `Temp/`.
- Managed file import is staged first, then committed, with rollback logic for partial commit failures.

## 6. Search

- Structured filtering is implemented in SQLite repository queries.
- Text search is backed by SQLite `FTS5` through the `search_index` virtual table.
- Search index maintenance is updated on repository add/remove operations.
- Repository integration tests already validate:
  - Cyrillic search
  - prefix matching
  - `е/ё` equivalence
  - text search across title, authors, tags, and description

## 7. Parsing

- `EPUB` parsing is implemented with `libzip` and `pugixml`.
- `FB2` parsing is implemented with `pugixml`.
- Parser dispatch is centralized in `ParserRegistry`.
- Parser tests cover both valid files and malformed metadata cases.

## 8. Converter Direction

- External conversion remains user-configurable and is not hard-wired to a single executable.
- The default supported converter direction is `FB2 -> EPUB`.
- The first built-in converter profile is based on `fb2cng` / `fbc`.
- User-specified converters are planned to work through an explicit argument-template contract rather than through one hard-coded binary interface.
- Converter configuration is modeled explicitly and currently supports three modes:
  - disabled
  - built-in `fb2cng`
  - custom external command
- The current implementation already contains command-building support for:
  - generic external converters with placeholders
  - the default `fb2cng` command shape
- External converter execution is implemented through a native process runner.
- The runtime currently supports two output modes:
  - exact destination file path
  - single produced file in a destination directory with relocation into the managed target path
- Converter execution supports cancellation through `stop_token` and progress reporting through `IProgressSink`.
- Conversion results now distinguish `succeeded`, `failed`, and `cancelled`.
- Import conversion policy is implemented as a separate slice:
  - `EPUB` input is stored without conversion
  - `FB2` input attempts conversion to `EPUB` when a compatible converter is available
  - failed or unavailable conversion falls back to storing the original `FB2` with warnings
  - cancelled conversion remains a cancellation outcome and does not silently fall back to storing the source file
- Single-file import orchestration is implemented for the non-UI core flow:
  - format detection and parsing
  - duplicate lookup
  - strict duplicate rejection
  - probable duplicate decision-required result unless forced
  - explicit probable duplicate override path
  - conversion planning and execution
  - managed storage preparation and commit
  - repository write and cleanup
  - rollback of prepared storage and repository state when late import steps fail
  - cleanup of temporary converter output when import finishes in cancellation
  - coordinator-level cancellation checks before late storage and persistence steps

## 9. ZIP Import

- ZIP import orchestration is implemented as a separate native slice.
- ZIP handling uses `libzip`.
- The current ZIP import flow:
  - enumerates archive entries
  - skips directory entries
  - skips nested `.zip` entries as unsupported
  - rejects unsafe archive entry paths such as traversal outside the extraction root
  - extracts supported `.fb2` and `.epub` files into a temp workspace
  - runs the existing single-file import pipeline per extracted entry
  - aggregates per-entry results without rolling back successful entries

## 10. External Converter Reference

The default converter profile is based on:

- `fb2cng` repository: <https://github.com/rupor-github/fb2cng>
- local reference guide used during implementation: `C:\Users\evgen\Downloads\guide.md`

Stable facts taken from that reference:

- the main executable is `fbc` / `fbc.exe`
- conversion command shape is `fbc convert [options] SOURCE [DESTINATION]`
- destination is a directory, not a fully explicit output filename
- output format is selected with `--to`
- overwrite support is provided by `--overwrite`
- optional YAML configuration can be passed with `-c <file>`

## 11. Testing Baseline

- Native tests use `Catch2`.
- The repository currently has unit and integration-style tests for:
  - domain rules
  - SQLite schema and wrappers
  - repositories
  - managed storage
  - EPUB parsing
  - FB2 parsing
  - parser registry
  - search behavior
  - converter command building
  - external converter runtime execution and cancellation
  - import conversion fallback policy
  - converter configuration model
  - single-file import orchestration
  - single-file import rollback and probable-duplicate override behavior
  - review regression coverage for id reservation, cancellation cleanup, and staged-file rollback
- ZIP import orchestration and partial-success aggregation
- application-level import facade routing and summary aggregation
- protobuf pipe framing and request dispatch over the job service adapter
- Win32 named-pipe message exchange over a live pipe
- end-to-end named-pipe host request/response loop over the import job service
- end-to-end typed named-pipe client calls over the import job service
- application-facing import job RPC calls over named pipes without direct protobuf usage in the caller
- process-level smoke coverage for the native host executable over a real named-pipe request flow
- native logging initialization and file output coverage
- C# pipe protocol round-trip and corruption-rejection coverage
- C# end-to-end import-job client coverage against the real native host process
- C# mapping and service-layer coverage for UI-facing import job DTOs
- C# shell bootstrap/session coverage over a real host-backed import flow
- C# ViewModel coverage for import job state and command enablement

## 12. Current Gaps

Not implemented yet, even if already planned architecturally:

- trash implementation
- full job engine with persistent job registry and streaming job state model
- richer long-running native host lifetime management beyond the current sequential session loop
- Avalonia UI workflow

## 13. Import Jobs

- The repository now contains a first `Jobs` slice above the application import facade.
- `ImportJobRunner` wraps one import request into a normalized job result shape with:
  - status
  - percent
  - message
  - warnings
  - optional aggregated import result
  - optional structured domain error
- The current job status model distinguishes:
  - pending
  - running
  - completed
  - failed
  - cancelled
- The current job runner maps import outcomes into stable higher-level job outcomes:
  - successful single-file import -> completed
  - partial-success ZIP import -> completed with partial-success message
  - probable-duplicate decision-required -> failed with structured domain error
  - cancellation -> cancelled with structured domain error
  - ZIP requests with no successfully imported books -> failed instead of false success
- `ImportJobManager` provides the first in-memory long-running job container with:
  - numeric job ids
  - background execution through `std::jthread`
  - live snapshot updates from `ImportJobRunner`
  - `start`
  - `try-get snapshot`
  - `try-get result`
  - `cancel`
  - `wait`
  - `remove` for completed jobs so finished records do not need to stay in memory forever
- `ImportJobManager` joins outstanding worker threads during destruction, so shutdown and test teardown do not leave background jobs running against destroyed dependencies.
- `ApplicationJobs` provides an application-facing service above the in-memory job manager:
  - start import job
  - query snapshot by job id
  - query final result by job id
  - cancel job
  - wait for completion
  - remove completed job
- The application-facing job service maps internal `Jobs` types into its own DTO layer so future gRPC and UI code do not need to depend directly on low-level job internals.
- Progress callback failures inside `ImportJobRunner` are ignored so observers cannot accidentally fail the import itself.
- Job-layer tests cover both single-file and ZIP-backed execution paths as well as in-memory job cancellation and completed-job cleanup.
