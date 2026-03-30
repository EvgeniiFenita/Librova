# LibriFlow Project Documentation

This document records the stable, already-implemented facts of the repository.

Unlike the architecture roadmap, this file should describe what is already true in the project today.

Update it when an implementation detail becomes stable enough to be treated as current project reality.

## 1. Product Baseline

- LibriFlow is a Windows-first desktop application for managing a personal e-book library.
- The MVP supports `EPUB`, `FB2`, and `ZIP` import.
- The UI is planned as `C# / Avalonia`.
- The core is implemented in `C++20`.
- The system architecture is two-process and uses `gRPC + Protobuf` at the process boundary.

## 2. Repository Baseline

- Native code is organized as one static library per logical slice under `libs/<SliceName>/`.
- UI applications live under `apps/`.
- Repository-level documentation lives under `docs/`.
- Build artifacts are routed under the repository root `out/`.
- `CMake` is the canonical native build system.

## 3. Implemented Native Slices

Implemented slices at this point:

- `Domain`
- `Core`
- `DatabaseSchema`
- `DatabaseRuntime`
- `Sqlite`
- `BookDatabase`
- `StoragePlanning`
- `ManagedStorage`
- `EpubParsing`
- `Fb2Parsing`
- `ParserRegistry`
- `SearchIndex`
- `ConverterCommand`
- `ConverterRuntime`
- `ImportConversion`

## 4. Persistence And Storage

- SQLite is the active local database technology.
- The schema includes `books`, `authors`, `book_authors`, `tags`, `book_tags`, `formats`, and `search_index`.
- SQLite connections enable `foreign_keys` per connection.
- Managed storage uses stable `BookId`-based paths under `Books/`, `Covers/`, and `Temp/`.
- Managed file import is staged first, then committed, with rollback logic for partial commit failures.

## 5. Search

- Structured filtering is implemented in SQLite repository queries.
- Text search is backed by SQLite `FTS5` through the `search_index` virtual table.
- Search index maintenance is updated on repository add/remove operations.
- Repository integration tests already validate:
  - Cyrillic search
  - prefix matching
  - `е/ё` equivalence
  - text search across title, authors, tags, and description

## 6. Parsing

- `EPUB` parsing is implemented with `libzip` and `pugixml`.
- `FB2` parsing is implemented with `pugixml`.
- Parser dispatch is centralized in `ParserRegistry`.
- Parser tests cover both valid files and malformed metadata cases.

## 7. Converter Direction

- External conversion remains user-configurable and is not hard-wired to a single executable.
- The default supported converter direction is `FB2 -> EPUB`.
- The first built-in converter profile is based on `fb2cng` / `fbc`.
- User-specified converters are planned to work through an explicit argument-template contract rather than through one hard-coded binary interface.
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

## 8. External Converter Reference

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

## 9. Testing Baseline

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

## 10. Current Gaps

Not implemented yet, even if already planned architecturally:

- ZIP import orchestration
- trash implementation
- application use cases and job engine
- protobuf contracts
- gRPC services
- Avalonia UI workflow
