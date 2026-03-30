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
- `2026-03-30` Feature: added reserved book id allocation and first single-file import orchestration with tested duplicate handling, conversion, storage staging, and repository write flow.
- `2026-03-30` Feature: strengthened single-file import with explicit probable-duplicate override flow and rollback coverage for repository/storage failures.
- `2026-03-30` Fix: closed review findings around book id reservation collisions, transactional repository removal, coordinator cancellation cleanup, and weak staged-file rollback tests.
- `2026-03-30` Feature: added ZIP import orchestration with entry extraction, nested-archive skip, and partial-success aggregation over the single-file import pipeline.
- `2026-03-30` Feature: added the first application-facing import facade that routes between single-file and ZIP flows and produces aggregated import summaries.
- `2026-03-30` Feature: added the first import job runner with normalized job status snapshots, structured error mapping, and ZIP-aware partial-success coverage.
- `2026-03-30` Feature: added the first in-memory import job manager with background execution, live snapshots, cancellation, and result lookup.
- `2026-03-30` Feature: added an application-facing import job service with dedicated DTOs above the in-memory job manager.
