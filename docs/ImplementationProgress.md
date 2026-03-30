# LibriFlow Implementation Progress

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
- `2026-03-30` Feature: validated real FTS search behavior for Cyrillic, prefix matching, and `е/ё` equivalence through repository integration tests.

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

- `2026-03-30` Feature: added the first `LibriFlow.UI` infrastructure for development-time core host path resolution, launch option modeling, and host process startup with named-pipe readiness checks.
- `2026-03-30` Test: added the first C# `xUnit` coverage for UI-side core host launch options, path resolution, and process startup readiness.
- `2026-03-30` Feature: added generated C# protobuf contracts and the first managed named-pipe framing implementation with `xUnit` coverage.
- `2026-03-30` Feature: added a managed named-pipe RPC client and the first C# import-job end-to-end flow against the real native host.
- `2026-03-30` Feature: added a UI-facing import-jobs service and DTO mapping layer with `xUnit` coverage and host-backed end-to-end service flow.
- `2026-03-30` Feature: added the first shell bootstrap/session layer for UI-side native host lifetime ownership with host-backed `xUnit` coverage.

### Phase 9. Stabilization

- `2026-03-30` Refactor: removed stale `ProtoGrpcServices` scaffolding and added the first native `spdlog`-backed logging facade with host file logging.
