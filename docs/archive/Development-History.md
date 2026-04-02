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
- `2026-04-01` Fix: added per-connection SQLite `busy_timeout` hardening and regression coverage for overlapping write transactions so the host does not fail immediately on short-lived write contention.
- `2026-04-01` Fix: replaced locale-dependent FB2 numeric parsing with exact locale-independent parsing and added regression coverage for dot-separated series numbers plus invalid publish-year rejection.
- `2026-04-01` Test: added explicit IPC enum invariants and cross-suite transport guardrails so C++ and C# named-pipe method and response-status ids cannot drift silently.
- `2026-04-01` Fix: made export overwrite behavior explicit in native logs, added rollback-failure diagnostics for trash restore paths, and enabled native `/utf-8` compilation so structured Unicode-safe logging stays available without per-call workarounds.
- `2026-04-01` Refactor: centralized managed-path safety validation and UTF-8 path conversion into a shared native helper slice, then re-verified export and trash flows with dedicated unit coverage.
- `2026-04-01` Cleanup: hardened local ZIP archive wrappers against accidental copy and move semantics, and narrowed import-job progress wakeups to `notify_one` on the single-waiter path.
- `2026-04-01` Cleanup: rewrote decoded FB2 XML declarations to `utf-8`, made search-index insert semantics explicit, added host `--help` and `--version` handling, and removed misleading UTF-8 helper naming in proto mappers.
- `2026-04-01` Fix: assigned external converter processes to kill-on-close job objects before resuming execution, made FB2 encoding-declaration rewrite case-insensitive, added host process smoke coverage for `--help` and `--version`, and synchronized the review-pass skill with the active MVP roadmap.
- `2026-04-01` Fix: restored live library search updates on each keystroke, made clearing the search box reload the full catalog immediately, and added ViewModel regression coverage plus a manual UI expectation update.
- `2026-04-01` Fix: changed export dialog suggestions to use sanitized `title + author` filenames instead of the generic `book` placeholder, with ViewModel regression coverage and updated product/manual UI docs.
- `2026-04-01` Fix: restored the import duplicate-override checkbox and re-verified through a host-backed UI test that probable-duplicate imports can create separate managed books with independent ids and files.
- `2026-04-01` Fix: widened duplicate override so import can continue even after strict duplicate detection, and added native plus host-backed regression coverage for independent duplicate records.
- `2026-04-01` Fix: changed library details size formatting from raw bytes to megabytes and added ViewModel regression coverage for the displayed metadata string.
- `2026-04-01` Fix: removed the redundant per-section hero header card from the main shell so `Library`, `Import`, and `Settings` open directly into their working surfaces.
- `2026-04-01` Fix: increased the spacing between the `Select Files...` and `Select Folder...` import actions to reduce visual crowding in the import source card.
- `2026-04-01` Fix: unified the `Select Files...` and `Select Folder...` import actions under the same primary styling so both source-selection paths read as equivalent actions.
- `2026-04-01` Fix: normalized shell button semantics by making `Save` a primary action and introducing a distinct destructive style for `Move To Trash`, while keeping navigation and browse flows secondary.
- `2026-04-01` Fix: added a dedicated `Browse...` picker for the built-in `fb2cng` executable path in `Settings`, wired through the shell path-selection abstraction with C# coverage.
- `2026-04-01` Fix: removed the MVP YAML config path from the built-in `fb2cng` settings flow and stopped the UI defaults layer from surfacing legacy saved config paths back into host launch options.
- `2026-04-01` Fix: added the same executable `Browse...` picker affordance to the custom converter settings path so both converter modes can be configured without manual path typing.
- `2026-04-01` Fix: moved `Settings` to a bottom-anchored position in the left navigation rail so system configuration is visually separated from the primary `Library` and `Import` workflows.
- `2026-04-01` Fix: replaced the technical left-rail subtitle with the product-facing slogan `A personal desktop library for your books.`
- `2026-04-01` Fix: added a first application icon asset and wired it into the Avalonia window plus the Windows application manifest path.
- `2026-04-01` Fix: removed the library-card hover opacity effect so pointer movement no longer dims the entire card and produces visible flicker across text and cover content.
- `2026-04-01` Fix: restored a clearer but stable library-card hover affordance through an outer shadow instead of whole-card opacity changes, keeping pointer feedback visible without flicker.
- `2026-04-01` Fix: refined library-card hover again to use rounded in-card highlight and border treatment aligned with the card shape, avoiding the rectangular shadow artifact behind rounded cards.
- `2026-04-01` Fix: removed the double-outline conflict between hover and selected library cards by suppressing hover chrome on selected items and keeping the non-selected base card border at zero thickness.
- `2026-04-01` Docs: replaced the mixed roadmap / MVP-remediation terminology with a single priority-based project backlog model, keeping the remaining manual-test remarks under one active backlog with `Critical`, `Major`, `Minor`, and `Low` priorities.
- `2026-04-01` Docs: renamed the active backlog document from `Librova-Roadmap.md` to `Librova-Backlog.md` and updated project instructions plus reusable skill checklists to reference the new canonical path.
- `2026-04-01` Docs: translated the active backlog document to English and cleaned up remaining plan / skill references so the repo consistently uses backlog-item terminology instead of roadmap buckets.
- `2026-04-01` Fix: added deterministic aggregate import progress across multi-file, directory, and ZIP import flows by carrying total/processed/imported/failed/skipped counts through the job snapshot transport, surfacing them in the UI running/result states, and re-verifying native plus managed regression coverage.
- `2026-04-01` Fix: corrected mixed batch + ZIP progress totals, made import workload preparation cancellation-aware, and stopped broken ZIP archives from failing entire mixed batch imports before valid sources are processed, with added native and host-backed regression coverage.
- `2026-04-01` Fix: changed batch-import cancellation semantics to roll back already imported managed books and covers so the library database and filesystem return to the pre-import state, with added host-backed regression coverage against the real core host.
- `2026-04-01` Fix: added per-source and per-entry host logging for skipped and failed batch, directory, and ZIP import items, with native plus host-backed regression checks against the real `host.log`.
- `2026-04-01` Feature: extended export so sessions with a configured converter can export managed `FB2` books as `EPUB`, added the `Export As EPUB` UI affordance gated by current-session converter availability, and re-verified native plus managed transport coverage.
- `2026-04-01` Fix: reloaded the active shell session after saving converter settings so `Export As EPUB` becomes usable immediately, and changed import conversion to an explicit `Force conversion to EPUB during import` checkbox that appears only when the current session has a configured converter, with native plus managed regression coverage.
- `2026-04-01` Fix: kept the active `Settings` section selected across converter-setting reloads, matched `Export As EPUB` styling to the primary export action, and redirected built-in `fb2cng` default log/report artifacts into `Library\Logs` so import temp folders and export destinations stay clean.
- `2026-04-02` Feature: added end-to-end aggregate library statistics for total managed-book count and total managed-book size in `MB`, exposed through protobuf/named pipes and surfaced in the left `Current Library` panel with native plus managed regression coverage.
- `2026-04-02` Fix: made library refresh resilient to statistics-RPC failures so the book grid still reloads when summary fetch fails, and added named-pipe plus managed regression coverage for the exact statistics contract and placement.
- `2026-04-02` Docs: aligned the `Library` search UX wording and project docs with the accepted global full-text search behavior, and closed the obsolete backlog item for a separate title/author-only search mode.
- `2026-04-02` Fix: enforced desktop minimum window dimensions derived from the `Library` layout, including scrollbar and window-chrome allowances, so opening `Selection Details` still leaves at least two visible book-card columns and the browser keeps at least two visible rows, with managed coverage for the invariants and updated manual UI expectations.
- `2026-04-02` Fix: changed `Library` cover presentation so real covers keep their aspect ratio on a neutral matte background while gradient placeholder backgrounds remain only for books without covers, with ViewModel regression coverage for both states.
- `2026-04-02` Fix: kept the selected library card in view across `Selection Details` open/close reflow, restoring the pre-open scroll position when still valid and otherwise falling back to a minimal visibility-preserving scroll adjustment, with managed geometry coverage and updated manual UI expectations.
