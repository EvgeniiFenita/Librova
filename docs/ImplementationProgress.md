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
