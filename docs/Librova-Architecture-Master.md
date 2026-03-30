# Librova Architecture Master Document

This document is the concise architecture source of truth for active work.

It should stay short and operational:

- what the product is;
- which architecture decisions are frozen;
- what the repository layout means;
- what remains on the roadmap.

Historical detail and the original full planning document are preserved in:

- `docs/archive/Librova-Architecture-Full.md`

## 1. Product Summary

Librova is a Windows-first desktop application for managing a personal library of e-books.

The MVP is intentionally narrower than Calibre:

- one user;
- one managed library;
- offline-first;
- import-centric workflow;
- strong Unicode and Cyrillic correctness;
- clean separation between UI and backend.

MVP-relevant formats:

- `EPUB`
- `FB2`
- `ZIP`

## 2. Frozen Decisions

### 2.1 Runtime Architecture

- The system is two-process:
  - `Librova.UI` in `C# / .NET / Avalonia`
  - `Librova.Core` in `C++20`
- UI and core communicate through `Protobuf` contracts over Windows named pipes.
- `gRPC` runtime is not on the MVP path.
- Direct `P/Invoke` is not the main architecture.

### 2.2 Build And Repository

- Native code is built with `CMake` and `vcpkg` manifest mode.
- `CMake` is the canonical build system for native code.
- Native code is organized as one static library per logical slice under `libs/<SliceName>/`.
- UI applications live under `apps/`.
- Shared protobuf contracts live under `proto/`.
- Verified progress is tracked in `docs/ImplementationProgress.md`.
- Stable implemented reality is tracked in `docs/ProjectDocumentation.md`.

### 2.3 Persistence, Storage, And Search

- SQLite is the local database.
- Managed storage lives under one library root with stable ID-based paths.
- Imports stage files before commit and must support rollback.
- Search is hybrid:
  - structured filters in relational tables
  - text search through SQLite `FTS5`
- Unicode correctness is mandatory at storage, search, and IPC boundaries.

### 2.4 Import And Conversion

- `EPUB` is stored as `EPUB`.
- `FB2` is converted to `EPUB` when conversion succeeds.
- If conversion is unavailable or fails, the original `FB2` may be stored with warnings.
- Conversion cancellation is distinct from conversion failure and must not silently fall back.
- Strict duplicates are rejected.
- Probable duplicates require explicit user confirmation.
- ZIP import supports partial success.

### 2.5 Logging And Testing

- Core logging uses `spdlog`.
- UI logging uses `Serilog`.
- Important execution paths in both C++ and C# must have actionable logs.
- Native tests use `Catch2`.
- UI-side tests use `xUnit`.
- New user-facing behavior should land with verification on the backend or UI side, or both, depending on the boundary touched.

## 3. Current Repository Shape

Current top-level layout:

- `apps/Librova.Core.Host`
- `apps/Librova.UI`
- `docs/`
- `libs/`
- `proto/`
- `scripts/`
- `tests/`

Key documentation split:

- `docs/Librova-Architecture-Master.md`
  concise current architecture and remaining roadmap
- `docs/ProjectDocumentation.md`
  implemented stable facts
- `docs/ImplementationProgress.md`
  verified checkpoints by phase
- `docs/archive/Librova-Architecture-Full.md`
  archived long-form original plan

## 4. Implemented Architecture Baseline

The following are already established:

- domain model, normalization, duplicate rules, and service/repository ports
- SQLite persistence, managed storage, migrations, and search indexing
- `EPUB` and `FB2` parsing
- converter command/config/runtime flow
- single-file and ZIP import orchestration
- in-memory jobs and application job service
- protobuf contracts and transport mapping
- named-pipe host/client transport
- native core host executable
- initial Avalonia shell with import-job workflow, logging, and path selection wiring

Details belong in `docs/ProjectDocumentation.md`, not here.

## 5. Remaining MVP Roadmap

### Phase 8. UI Shell

The UI shell is no longer just an import placeholder. The current remaining Phase 8 work should stay focused on product-critical user flows:

- current Phase 8 UI baseline is functionally complete for MVP and should only take targeted cleanup tied to stabilization

### Phase 9. Stabilization

Still expected before MVP completion:

- broad review passes on runtime behavior, crash safety, and tests
- polish of logging coverage and error surfaces
- maintenance of a current manual UI test checklist for release-candidate validation
- cleanup of docs and remaining architectural drift
- final release-candidate review and targeted fix pass

### Functional MVP Gaps Still Not Done

Important planned capabilities that are not yet implemented end-to-end:
- no major functional MVP gaps remain; remaining work is stabilization and release-candidate hardening

## 6. Working Rule For This Document

Keep this file concise.

If a section starts turning into implementation inventory, move that detail to:

- `docs/ProjectDocumentation.md`, or
- `docs/archive/Librova-Architecture-Full.md`

Only update this file when a change affects active architectural direction, frozen decisions, or the remaining roadmap.

Do not keep already-implemented UI or backend slices listed as roadmap gaps; move those facts into `docs/ProjectDocumentation.md` and keep this file focused on the real remaining MVP work.
