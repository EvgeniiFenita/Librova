# Librova Implementation Progress

This document tracks completed, verified checkpoints so the project does not revisit already-finished bootstrap work by accident.

Update it when a logical step is finished, verified, and ready to be treated as established project state.

## Completed Checkpoints

### Phase 0. Build Foundation

- `2026-03-30` `6ef0a62` Docs: bootstrapped repository guidelines and ignore rules.
- `2026-03-30` `100fd3c` Build: defined repository line ending policy with `.gitattributes` and `.editorconfig`.
- `2026-03-30` `d349fb8` Build: bootstrapped core and UI project structure.
- `2026-03-30` `3061001` Docs: moved project documentation into `docs/`.
- `2026-03-30` `6d1e883` Build: validated native test toolchain with Visual Studio 2026 `vcpkg` and `Catch2`.
- `2026-03-30` `55fcce7` Refactor: reorganized repository into library slices with `libs/`, `apps/`, `proto/`, and `tests/`.
- `2026-03-30` `7afd6f9` Build: routed `.NET` build artifacts into repository-root `out/`.

### Phase 1. Domain Core

- `2026-03-30` `efb9847` Feature: added initial domain value objects for `BookId`, `BookFormat`, and duplicate classification.
- `2026-03-30` `b46c725` Feature: added `BookMetadata`, `BookFileInfo`, and `Book` aggregate skeleton.
- `2026-03-30` Feature: added domain error model with explicit error codes and classification helpers.
- `2026-03-30` Feature: added initial domain query types and repository ports.
- `2026-03-30` Feature: added remaining domain service contracts for parser, converter, storage, trash, cover, and progress reporting.
- `2026-03-30` Feature: added metadata normalization rules for text, ISBN, and duplicate keys.

### Phase 2. Persistence And Managed Storage

- `2026-03-30` Feature: added managed library storage planning and stable path layout rules.
- `2026-03-30` Feature: added versioned database schema definitions and migration statements.
- `2026-03-30` Feature: added SQLite RAII wrappers and temporary-database smoke coverage.
- `2026-03-30` Feature: added schema migration runner with `user_version` handling and idempotence coverage.
- `2026-03-30` Feature: added first SQLite book repository with transactional add/get/remove behavior and round-trip coverage.
- `2026-03-30` Feature: added SQLite query repository with structured search filters and duplicate classification.
- `2026-03-30` Feature: added managed file storage with staging, commit, and rollback coverage for book and cover files.

### Phase 3. Parsers

- `2026-03-30` Feature: added EPUB parser with metadata and cover extraction backed by `libzip` and `pugixml`.
- `2026-03-30` Feature: added FB2 parser with metadata and embedded cover extraction backed by `pugixml`.
- `2026-03-30` Feature: added parser registry with format detection and dispatch across EPUB and FB2 parsers.

### Phase 4. Search Layer

- `2026-03-30` Feature: added SQLite FTS index maintenance and routed text search through `search_index` for title, authors, tags, and description.
- `2026-03-30` Feature: validated real FTS search behavior for Cyrillic, prefix matching, and Cyrillic `e/yo` equivalence through repository integration tests.

### Phase 5. Converter Integration

- `2026-03-30` Feature: added converter command foundation with a built-in `fb2cng` profile and a user-configurable argument-template contract.
- `2026-03-30` Feature: added external converter runtime execution with process cancellation support and output relocation for directory-based converters.
- `2026-03-30` Feature: added explicit conversion result statuses and a separate import conversion policy with tested fallback and cancellation behavior.
- `2026-03-30` Feature: added explicit converter configuration modes for disabled, built-in `fb2cng`, and custom command profiles.

### Phase 6. Use Cases And Job Engine

- `2026-03-30` Feature: added reserved book id allocation and first single-file import orchestration with tested duplicate handling, conversion, storage staging, and repository write flow.
- `2026-03-30` Feature: strengthened single-file import with explicit probable-duplicate override flow and rollback coverage for repository/storage failures.
- `2026-03-30` Fix: closed review findings around book id reservation collisions, transactional repository removal, coordinator cancellation cleanup, and weak staged-file rollback tests.
- `2026-03-30` Feature: added ZIP import orchestration with entry extraction, nested-archive skip, and partial-success aggregation over the single-file import pipeline.
- `2026-03-30` Feature: added the first application-facing import facade that routes between single-file and ZIP flows and produces aggregated import summaries.
- `2026-03-30` Feature: added the first import job runner with normalized job status snapshots, structured error mapping, and ZIP-aware partial-success coverage.
- `2026-03-30` Feature: added the first in-memory import job manager with background execution, live snapshots, cancellation, and result lookup.
- `2026-03-30` Feature: added an application-facing import job service with dedicated DTOs above the in-memory job manager.
- `2026-03-30` Fix: hardened ZIP import paths, empty-import job semantics, progress callback isolation, and completed-job cleanup APIs.

### Phase 7. IPC Surface

- `2026-03-30` Feature: added the first shared protobuf contract for import jobs and `LibraryJobService`.
- `2026-03-30` Build: wired protobuf C++ code generation into the native build and added round-trip message tests.
- `2026-03-30` Feature: added protobuf transport mapping between application job DTOs and generated contracts.
- `2026-03-30` Feature: added the first protobuf service adapter over `ApplicationJobs` for `LibraryJobService` operations.
- `2026-03-30` Feature: extended the shared protobuf and named-pipe surface with read-side `ListBooks`, a dedicated catalog proto mapper, and end-to-end adapter/dispatcher coverage against the real SQLite query path.
- `2026-03-30` Build: enabled protobuf tooling through manifest `vcpkg` and added a local `protoc` validation script.
- `2026-03-30` Docs: revised the IPC baseline from mandatory `gRPC` runtime to transport-neutral `Protobuf over Windows named pipes` for the MVP.
- `2026-03-30` Feature: added the first pipe transport foundation with binary envelope framing and protobuf request dispatch over the job service adapter.
- `2026-03-30` Feature: added the first Win32 named-pipe channel with real message round-trip coverage.
- `2026-03-30` Fix: hardened import job manager teardown by joining worker threads and removed nondeterministic test-process crashes during shutdown.
- `2026-03-30` Feature: added the first named-pipe host loop with end-to-end request dispatch over the protobuf job service.
- `2026-03-30` Feature: added the first typed named-pipe client helper with end-to-end request/response coverage.
- `2026-03-30` Feature: added an application-facing import job client over the named-pipe transport and reverse protobuf mapping.
- `2026-03-30` Feature: added the first native core host executable with CLI options, startup migration, and sequential named-pipe service loop.
- `2026-03-30` Fix: hardened named-pipe runtime with startup recovery cleanup, per-session host error isolation, RPC response timeout handling, and process-level host smoke coverage.

### Phase 8. UI Shell

- `2026-03-30` Feature: added the first application-facing catalog facade for read-side book listing and verified it against the real SQLite query repository for filtering, pagination, and DTO mapping.
- `2026-03-30` Feature: added the first `Librova.UI` infrastructure for development-time core host path resolution, launch option modeling, and host process startup with named-pipe readiness checks.
- `2026-03-30` Test: added the first C# `xUnit` coverage for UI-side core host launch options, path resolution, and process startup readiness.
- `2026-03-30` Feature: added generated C# protobuf contracts and the first managed named-pipe framing implementation with `xUnit` coverage.
- `2026-03-30` Feature: added a managed named-pipe RPC client and the first C# import-job end-to-end flow against the real native host.
- `2026-03-30` Feature: added a UI-facing import-jobs service and DTO mapping layer with `xUnit` coverage and host-backed end-to-end service flow.
- `2026-03-30` Feature: added the first shell bootstrap/session layer for UI-side native host lifetime ownership with host-backed `xUnit` coverage.
- `2026-03-30` Feature: added the first ViewModel-ready shell and import-job state layer with observable state, async commands, and `xUnit` coverage.
- `2026-03-30` Feature: added a shell-application composition layer that materializes a ready `ShellViewModel` from a running shell session.
- `2026-03-30` Fix: hardened UI command and import-job state handling to avoid fire-and-forget updates, keep polling until terminal completion, and convert command failures into controlled ViewModel state.
- `2026-03-30` Feature: added the first real Avalonia app skeleton with `App`, `MainWindow`, shell-window composition, and verified C# coverage for window-state configuration.
- `2026-03-30` Feature: expanded the UI shell with explicit refresh and cancel import-job actions in the ViewModel and bound Avalonia window controls, with `xUnit` coverage for both behaviors.
- `2026-03-30` Feature: added an explicit shell-window startup state model for both running and startup-error modes, with verified C# coverage for both compositions.
- `2026-03-30` Feature: added derived import-result presentation state in the UI shell for summary, warnings, and error text, with verified `xUnit` coverage through the ViewModel layer.
- `2026-03-30` Feature: added completed-job removal to the UI shell with local result-state cleanup and verified `xUnit` coverage for command behavior and state reset.
- `2026-03-30` Feature: added a testable desktop path-selection abstraction for source and working-directory browse commands in the UI shell, with verified `xUnit` coverage through the ViewModel layer.
- `2026-03-30` Feature: wired a real Avalonia path-selection adapter into shell composition so browse commands can use the desktop storage provider while remaining testable.
- `2026-03-30` Feature: added deterministic default import working-directory initialization under the managed library temp area, with verified shell-composition coverage.
- `2026-03-30` Feature: exposed probable-duplicate override in the UI import shell and verified that the flag is propagated through the ViewModel request model.
- `2026-03-30` Feature: added shell launch-argument handling so the UI can start with a prefilled source file path, with verified C# coverage for argument parsing and shell composition.
- `2026-03-30` Feature: added persisted UI shell state for source path, working directory, and probable-duplicate override, with verified C# coverage for JSON state-store round-trip and shell lifecycle persistence.
- `2026-03-30` Feature: added drag-and-drop source-file support in the Avalonia shell and verified the dropped-path application behavior through C# ViewModel tests.
- `2026-03-30` Build: added `Run-Librova.ps1` as a one-click development entry point that performs a full build, prepares `out\\runtime`, and launches the UI with runtime logs/state redirected out of `%LOCALAPPDATA%`.
- `2026-03-30` Feature: reworked the Avalonia import shell into a more structured desktop layout with dedicated workspace, session, summary, and warning/error panels while preserving the existing tested ViewModel flow.
- `2026-03-30` Fix: changed the UI host-readiness probe to wait for pipe availability without opening a throwaway connection, eliminating false broken-pipe startup errors from the native host log.
- `2026-03-30` Fix: hardened the UI import shell by removing the hardcoded 15-second auto-cancel path, making shell-state persistence best-effort during shutdown, adding local path validation for import inputs, and reducing polling-related UI log noise.
- `2026-03-30` Feature: added the first persisted next-launch settings flow for `PreferredLibraryRoot`, with UI save/reset actions, default-host bootstrap integration, and C# coverage for preferences round-trip and shell settings behavior.
- `2026-03-30` Feature: added an explicit diagnostics panel to the UI shell with runtime log/state/executable paths and verified shell-level coverage for the exposed diagnostics values.
- `2026-03-30` Feature: added an explicit operational-notes layer to the UI shell so launch-argument prefill, next-launch library-root mismatch, and runtime redirection hints are visible in the running session.
- `2026-03-30` Feature: enriched the startup-error screen with actionable guidance plus UI diagnostics paths for log, state, and preferences files.
- `2026-03-30` Feature: added the first UI-side library catalog client/service, wired it into shell sessions, and exposed a refreshable library snapshot panel with host-backed C# coverage and browser ViewModel tests.
- `2026-03-30` Feature: expanded the UI library snapshot into a real browser slice with filters, sort selection, page navigation, and selected-book details while keeping the flow covered by C# ViewModel tests.
- `2026-03-30` Feature: linked import and browser flows so the library view preloads on shell startup and refreshes automatically after successful imports, with C# shell/viewmodel coverage for both paths.
- `2026-03-30` Test: added strong host-backed UI integration coverage for shell preload, real import-to-browser refresh, and browser pagination through the native host and SQLite catalog path.
- `2026-03-30` Feature: added end-to-end read-side book details lookup over protobuf/named pipes and surfaced explicit full-details loading in the UI library browser with native and C# coverage.
- `2026-03-30` Feature: added end-to-end managed-book export from the library browser over protobuf/named pipes, including native managed-path safety checks, C# path-selection wiring, and strong host-backed integration coverage.
- `2026-03-30` Feature: added end-to-end delete-to-trash from the library browser over protobuf/named pipes, including managed-library trash policy, rollback-friendly native deletion semantics, and strong host-backed integration coverage.
- `2026-03-30` Build: generalized UI-side core-host resolution beyond `Debug` only and added `Run-Tests.ps1` as a root-level sequential full test runner for native and managed suites.
- `2026-03-30` Fix: hardened the UI shell by making Avalonia startup/shutdown non-blocking, adding an explicit loading state, switching browser pagination to lookahead-based `HasMoreResults`, clamping invalid page sizes, and replacing optimistic import ViewModel tests with validated command-path coverage.
- `2026-03-30` Feature: added a first-run setup state before shell startup so the user chooses and persists the managed library root before launching the native host, with dedicated validation, browse flow, and C# coverage for setup policy and setup ViewModel behavior.
- `2026-03-30` Feature: restructured the running Avalonia shell into a real application layout with separate `Library`, `Import`, and `Settings` sections instead of one overloaded dashboard surface, while preserving the existing import and browser ViewModel flows.
- `2026-03-30` Feature: added next-launch converter configuration to the UI settings flow, including persisted preferences, host-launch argument generation for built-in and custom modes, and C# coverage for converter preference round-trip and launch-option bridging.

### Phase 9. Stabilization

- `2026-03-30` Refactor: removed stale `ProtoGrpcServices` scaffolding and added the first native `spdlog`-backed logging facade with host file logging.
- `2026-03-30` Feature: added the first UI `Serilog`-backed logging baseline with file/debug sinks, lifecycle logging across host bootstrap and import-job service calls, and `xUnit` file-output coverage.
- `2026-03-30` Docs: archived the original long-form architecture plan and reduced `docs/Librova-Architecture-Master.md` to a concise active architecture summary with only frozen decisions and the remaining roadmap.
- `2026-03-30` Docs: resynchronized the active roadmap, stable project documentation, and session rules so only the real remaining MVP gaps stay on the forward path.
- `2026-03-30` Docs: added permanent MVP scope, feature-playbook, test-strategy, transport-invariant, and review-checklist documents to standardize future vertical slices and reduce roadmap drift.
- `2026-03-30` Fix: corrected the restructured UI shell so section switching affects the main content area correctly and the running-shell workspace uses vertical scrolling instead of overlapping views.
- `2026-03-30` Build: validated the full `x64-release` path, fixed the native process-level host test to work outside `Debug`, hardened `Run-Tests.ps1` to stop on failing external commands, and verified `Run-Librova.ps1` against `Release` runtime paths without launching the UI.
- `2026-03-30` Fix: hardened startup recovery by persisting first-run library settings only after successful shell startup, exposing a recovery setup path on the startup-error screen, and aligning development defaults with the injected UI preferences store.
- `2026-03-30` Docs: added a maintained manual UI validation checklist for first-run setup, startup recovery, shell navigation, import, browser, settings, diagnostics, and release-build smoke verification.
- `2026-04-01` Fix: made SQLite database opening Unicode-safe on Windows by switching native connection paths to explicit UTF-8 and added regression coverage for Cyrillic database paths.
- `2026-04-01` Fix: replaced watchdog-driven `ExitProcess` host termination with graceful shutdown signaling and added process-level verification that parent-process death still stops the host with flushed logs.
- `2026-04-01` Fix: bound external converter child processes to Windows kill-on-close job objects and added cancellation cleanup coverage so partial output files do not survive aborted conversions.
