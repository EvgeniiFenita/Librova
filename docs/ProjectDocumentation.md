# Librova Project Documentation

This document records the stable, already-implemented facts of the repository.

Unlike the architecture roadmap, this file should describe what is already true in the project today.

Update it when an implementation detail becomes stable enough to be treated as current project reality.

## 1. Product Baseline

- Librova is a Windows-first desktop application for managing a personal e-book library.
- The MVP supports `EPUB`, `FB2`, and `ZIP` import.
- The UI is planned as `C# / Avalonia`.
- `apps/Librova.UI` now also contains the first non-visual UI infrastructure for resolving and launching the native core host process during development.
- `apps/Librova.UI` now also generates C# protobuf contracts from `proto/import_jobs.proto` and contains the first managed implementation of the named-pipe framing protocol.
- `apps/Librova.UI` now also contains a real managed named-pipe RPC client and an import-job client over generated protobuf contracts.
- `apps/Librova.UI` now also contains a UI-facing import-jobs service and DTO layer that hides generated protobuf types from future ViewModels.
- `apps/Librova.UI` now also contains the first shell bootstrap/session layer that owns native host lifetime and exposes UI-facing services.
- `apps/Librova.UI` now also contains the first ViewModel-ready shell and import-jobs state layer with observable state and async command support.
- `apps/Librova.UI` now also contains a shell-application composition layer that turns a `ShellSession` into a ready-to-bind `ShellViewModel`.
- the current UI ViewModel baseline now keeps import jobs in an explicit polling state until a terminal result arrives instead of dropping back to idle immediately after job creation.
- the current `AsyncCommand` baseline captures command failures into controlled UI state instead of letting exceptions escape as unhandled command crashes.
- `apps/Librova.UI` now also contains the first UI logging baseline backed by `Serilog`, with file logging and debug output for startup, host lifecycle, IPC job calls, and command failure paths.
- `apps/Librova.UI` now also contains the first real Avalonia application skeleton with `App`, `MainWindow`, and shell-window composition over the existing native-host-backed `ShellApplication`.
- the current Avalonia shell baseline now exposes explicit `Start`, `Refresh`, and `Cancel` import-job actions through the `ImportJobsViewModel` and bound window controls.
- the current Avalonia shell baseline now has an explicit startup window state model with separate running and startup-error modes instead of ad hoc exception text rendering in `App`.
- the current Avalonia shell baseline now exposes derived result presentation state for import summary, warnings, and error text instead of leaving the latest job result as a raw DTO-only object graph.
- the current Avalonia shell baseline now supports removing a completed import job from the in-memory job registry and clearing the UI-side result state after successful removal.
- the current Avalonia shell baseline now exposes a testable path-selection abstraction for source-file and working-directory picking, so future desktop dialogs can stay outside the ViewModel layer.
- the current Avalonia shell baseline now includes a real Avalonia-backed path-selection adapter wired into shell composition, so `Browse...` actions can use the desktop storage provider while the ViewModel stays dialog-agnostic.
- the current shell composition now pre-populates the import `WorkingDirectory` with a deterministic path under `<LibraryRoot>/Temp/UiImport`, so the import screen is usable without manual temp-path entry.
- the current Avalonia shell baseline now exposes an explicit `AllowProbableDuplicates` toggle, so probable-duplicate override can be chosen from the UI instead of only through backend contracts.
- the current Avalonia shell baseline now accepts an initial source-file path from application launch arguments and pre-fills the import screen with that path on startup.
- the current Avalonia shell baseline now persists import-screen state between launches in a JSON state file under `%LOCALAPPDATA%\\Librova`, including source path, working directory, and probable-duplicate override.
- the current Avalonia shell baseline now accepts a dropped local file on the main window and applies it as the import `SourcePath`, so desktop drag-and-drop works without bypassing the ViewModel layer.
- the current Avalonia shell baseline now uses a two-column desktop layout with an explicit import workspace, session summary, operational cards, and a visible drag-and-drop callout instead of the earlier raw form-like shell.
- the current import shell validates `SourcePath` and `WorkingDirectory` locally before enabling `Start Import`, including file existence, supported source extensions, absolute-path expectations, and rejection of file paths in the working-directory field.
- the current import shell no longer auto-cancels a long-running job after a hardcoded 15-second UI timeout; cancellation is now user-driven while each IPC call still uses its own bounded timeout.
- shell shutdown now treats state persistence as best-effort: a failure to save UI shell state is logged but does not prevent disposal of the native host process.
- high-frequency import polling no longer emits success-path info logs for every snapshot/result query, which keeps `ui.log` focused on actionable events instead of routine polling noise.
- `tests/Librova.UI.Tests` now provides the first C# test baseline over UI-side core-host launch infrastructure.
- The core is implemented in `C++20`.
- The system architecture is two-process and uses `Protobuf` contracts at the process boundary.
- The current MVP transport direction is `Protobuf over Windows named pipes`, not mandatory `gRPC`.

## 2. Repository Baseline

- Native code is organized as one static library per logical slice under `libs/<SliceName>/`.
- UI applications live under `apps/`.
- The repository now also contains the first native host executable under `apps/Librova.Core.Host`.
- Repository-level documentation lives under `docs/`.
- `docs/Librova-Architecture-Master.md` is now the concise active architecture summary, while the original long-form planning document lives in `docs/archive/Librova-Architecture-Full.md`.
- Shared protobuf contracts live under `proto/`.
- Build artifacts are routed under the repository root `out/`.
- `CMake` is the canonical native build system.
- the repository root now contains [Run-Librova.ps1](C:\Users\evgen\Desktop\LibriFlow\Run-Librova.ps1) as the one-click development entry point for full build and UI launch.

## 3. Application Layer

- A first application-facing import facade is implemented.
- A first application-facing catalog facade is implemented for read-side book listing.
- The facade dispatches between:
  - single-file import
  - ZIP archive import
- The facade returns one aggregated result shape for higher layers instead of exposing low-level coordinator types directly.
- The catalog facade maps repository search results into lighter list DTOs for future UI and transport read-side flows instead of exposing full domain `SBook` objects directly.

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

- The repository now contains the first shared protobuf contract file in [proto/import_jobs.proto](C:\Users\evgen\Desktop\Librova\proto\import_jobs.proto).
- Protobuf package baseline is `librova.v1`.
- `.proto` file naming is now fixed as `snake_case`.
- The first contract covers:
  - import request DTO
  - book list request DTO
  - book list item DTO
  - import summary DTO
  - import job snapshot DTO
  - import job result DTO
  - `LibraryJobService` RPC surface for start/get/wait/cancel/remove
- The shared contract now also exposes `ListBooks` for read-side catalog queries over the same named-pipe/protobuf boundary.
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
  - `ListBooks`
  - `GetImportJobSnapshot`
  - `GetImportJobResult`
  - `WaitImportJob`
  - `CancelImportJob`
  - `RemoveImportJob`
- `libs/ProtoMapping` now also contains a dedicated catalog mapper for `BookListRequest`, `BookListItem`, and `ListBooksResponse`.
- The current adapter is transport-neutral and uses protobuf request/response types directly, but it does not yet depend on a real gRPC server runtime.
- The repository intentionally does not require a `gRPC C++` runtime for the MVP transport path.
- `libs/PipeTransport` now provides the first transport foundation for `Protobuf over named pipes`:
  - binary request/response envelope framing
  - explicit pipe method ids for `LibraryJobService`
  - protobuf request dispatch into the transport-neutral service adapter
  - structured transport statuses for invalid request, unknown method, and internal error
  - synchronous Win32 named-pipe channel wrappers with length-prefixed message exchange
  - real round-trip coverage against a live Windows named pipe
- The current named-pipe method set now covers both import-job operations and read-side `ListBooks` queries.
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
- `apps/Librova.Core.Host` now builds the first real native host executable:
  - ensures managed library directories exist
  - clears stale `Temp/` state during startup recovery
  - migrates SQLite schema on startup
  - composes parser registry, repositories, managed storage, import facade, job service, protobuf adapter, and named-pipe host
  - serves sequential named-pipe sessions according to host options
  - treats malformed or abruptly closed pipe sessions as per-session errors instead of terminating the whole host process
- the current pipe client timeout is applied as an RPC deadline for waiting on the response after connect, not only as an initial connect timeout
- `libs/Logging` now provides the first native logging facade over `spdlog`
- the current core logging baseline writes host logs into `Logs/host.log` and mirrors the same records to `stderr`
- the current UI logging baseline writes logs into `%LOCALAPPDATA%\\Librova\\Logs\\ui.log` and mirrors the same records to the debug sink
- when the UI is launched through `Run-Librova.ps1`, runtime files are redirected into `out\\runtime` through explicit environment overrides:
  - `out\\runtime\\logs\\ui.log`
  - `out\\runtime\\ui-shell-state.json`
  - `out\\runtime\\ui-preferences.json`
  - `out\\runtime\\library\\...`
  - `out\\runtime\\library\\Logs\\host.log`
- the current UI host-readiness check uses `WaitNamedPipe` and no longer creates a throwaway client connection that pollutes `host.log` with false broken-pipe startup errors
- the current UI shell contains the first explicit next-launch settings flow for `PreferredLibraryRoot`; users can browse, save, and reset the library root that future app launches should use
- the current UI shell now exposes a dedicated diagnostics panel with the active UI log path, host log path, UI state file, preferences file, and host executable path, so runtime inspection no longer requires guessing the current file locations
- the current UI shell now exposes an `Operational Notes` panel that surfaces launch-argument prefill, next-launch library-root mismatch, and runtime-redirection hints directly in the running shell instead of leaving them implicit
- the current startup-error screen now includes actionable guidance and the current UI log/state/preferences file paths, so bootstrap failures can be diagnosed without leaving the error screen

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
- application-level catalog facade mapping and pagination/filter integration over the real SQLite read side
- protobuf catalog mapping and `ListBooks` adapter/dispatcher coverage over the real SQLite read side
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
- C# shell-application composition coverage
- C# regression coverage for command error handling and terminal import polling behavior
- C# logging initialization and file-output coverage
- C# shell-window composition coverage without depending on a live Avalonia windowing platform in the test process
- C# ViewModel coverage for refresh and cancellation actions in the UI import shell
- C# shell-window state coverage for both normal startup composition and startup-error composition
- C# ViewModel coverage for derived import result presentation state
- C# ViewModel coverage for completed-job removal and result-state reset behavior
- C# ViewModel coverage for path-selection commands through a fake desktop interaction service
- C# shell composition coverage for injecting a desktop path-selection service into the running shell
- C# shell composition coverage for default import working-directory initialization
- C# ViewModel coverage for passing the probable-duplicate override flag through the UI import request
- C# coverage for shell launch-argument parsing and startup source-path prefill in shell composition
- C# coverage for persisted shell-state loading and saving across shell-application lifecycle
- C# ViewModel coverage for source-file drag-and-drop path application
- C# ViewModel coverage for local import-input validation and command enablement against real temporary files
- C# shell-lifecycle coverage for disposing the native host even when UI shell-state persistence fails
- C# coverage for persisted UI preferences and next-launch library-root settings behavior
- C# shell composition coverage for diagnostics-path exposure in the running session
- C# shell composition coverage for operational-warning visibility in the running session
- C# startup-error-state coverage for diagnostics-path and guidance exposure

## 12. Current Gaps

Not implemented yet, even if already planned architecturally:

- trash implementation
- full job engine with persistent job registry and streaming job state model
- richer long-running native host lifetime management beyond the current sequential session loop
- Avalonia UI workflow
- rich multi-screen Avalonia UI workflow beyond the current single-window shell

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
