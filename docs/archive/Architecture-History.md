# Librova Architecture Master Document

## 1. Overview

Librova is a desktop application for managing a personal library of e-books. The main reference point is Calibre, but the target product is intentionally narrower for the first release: simpler scope, clearer architecture, stronger separation between UI and core logic, and a test-driven native backend.

The system is designed for:

- Windows-only initial release scope;
- single user;
- exactly one managed library;
- offline operation;
- modern responsive UI;
- strong Unicode support, especially Cyrillic;
- clean separation between C# UI and C++ core;
- future portability to Linux/macOS after the first release.

## 2. Initial Release Scope

### Included

- import of single books;
- bulk import;
- import from `.zip` archives;
- metadata and cover extraction;
- storage of normalized metadata in a local database;
- managed library storage on disk;
- automatic conversion of non-EPUB input through an external converter command;
- search, filtering, sorting;
- export to a user-selected folder;
- delete to trash;
- right-side details panel;
- progress reporting and cancellation for long-running jobs;
- first-run setup wizard.

### Excluded from Initial Release Scope

- reading books inside the app;
- metadata editing;
- cloud sync;
- multiple libraries;
- collections, shelves, ratings, favorites;
- external metadata downloading as a real feature.

These excluded items should remain supported as future extension points in the architecture.

## 3. Functional Rules

### 3.1 Supported formats

Input formats in the initial release scope:

- `EPUB`
- `FB2`
- `ZIP`

ZIP rules:

- ZIP may contain folders;
- ZIP may contain `EPUB` and `FB2` files;
- nested archives are not supported;
- import result is partial-success capable.

### 3.2 Library storage rule

The initial release scope stores one managed file per logical book.

- imported `EPUB` is stored as `EPUB`;
- imported `FB2` is converted to `EPUB` when conversion succeeds;
- if conversion is unavailable or fails, original `FB2` is stored and used as the preferred format.

This keeps the aggregate simple while still leaving room for future multi-format support.

### 3.3 Duplicate detection

Duplicate detection uses two levels.

Strict duplicate:

- same file hash;
- same ISBN, when ISBN is present.

Probable duplicate:

- same normalized title;
- same normalized authors.

Behavior:

- strict duplicate rejects import;
- probable duplicate raises a warning and requires user confirmation for forced import.

### 3.4 Metadata model

Normalized metadata for the initial release scope includes:

- title;
- authors;
- language;
- series;
- series index;
- publisher;
- year;
- ISBN;
- tags;
- description;
- cover;
- identifiers.

Rules:

- authors are normalized into a separate entity with many-to-many relation;
- one language per book in the initial release scope;
- no metadata history;
- no raw metadata snapshot in the domain model;
- cover is stored as a file on disk.

### 3.5 Search, filtering, sorting

Required search behavior:

- case-insensitive;
- Cyrillic `e` and `yo` treated as equivalent;
- prefix search supported;
- no morphology or stemming in the initial release scope.

Required filters:

- author;
- language;
- tags;
- series;
- format;
- date added.

Required sorting:

- title;
- author;
- date added;
- series;
- year;
- file size.

Expected scale is a few thousand books, so maintainability and correctness are more important than optimization for very large datasets.

## 4. UX Constraints

- UI framework is Avalonia.
- UI language is English in the initial release scope.
- book metadata must support Unicode and Cyrillic correctly;
- main library view must support a cover grid;
- drag and drop must work for files, folders, and ZIP archives;
- book details open in a right-side panel;
- setup wizard configures library root and converter command;
- import, export, conversion, and verification must not block the UI;
- long-running jobs must report progress and support cancellation.

## 5. Architectural Vision

Librova should be implemented as a two-process desktop system:

- `Librova.UI` in C# / .NET / Avalonia;
- `Librova.Core` in C++20.

The UI handles presentation and user interaction. The core handles business logic, metadata extraction, conversion orchestration, persistence, file management, indexing, and integrity operations.

### Why two processes

- stronger separation between presentation and backend;
- crash isolation for native parsers and converter orchestration;
- stable service contract for future cross-platform frontends;
- no fragile native ABI coupling between .NET and C++;
- easier future headless or automation scenarios.

## 6. Technology Stack

| Area | Choice | Reason |
| --- | --- | --- |
| Backend language | C++20 | Modern feature set without unnecessary dependency on C++23 |
| Frontend language | C# / .NET | Natural fit for modern desktop UI |
| UI framework | Avalonia | Chosen explicitly; future cross-platform path |
| Build system | CMake + vcpkg manifest mode | Clean dependency management for native code |
| Local database | SQLite | Simplest robust local database |
| IPC contract | Protobuf | Stable, explicit schema boundary |
| IPC implementation | Custom request/response RPC over Windows named pipes | Pragmatic Windows-first transport without mandatory gRPC runtime |
| XML parsing | pugixml | Practical for FB2 and EPUB metadata parsing |
| ZIP handling | libzip | Needed for EPUB container access and ZIP import |
| Logging | spdlog in core, Serilog in UI | Mature and practical |
| Core unit tests | Catch2 | Explicit requirement |
| UI tests | xUnit | Good fit for ViewModel testing |

Deliberate decisions:

- no C++ Modules;
- no C++/CLI;
- no in-process native interop as the main architecture;
- no mandatory gRPC runtime in the initial release transport path;
- external converter is not bundled by architecture, it is configured by the user.
- `fb2cng` / `fbc` is the first built-in converter profile, but converter integration must remain user-configurable through an explicit command-template contract.

## 7. Layered Design

Librova follows Clean Architecture with clear boundaries:

- Domain;
- Application;
- Infrastructure;
- Presentation.

### 7.1 Domain

Contains:

- aggregates;
- value objects;
- metadata normalization rules;
- duplicate model;
- business errors;
- repository and service interfaces.

Domain must not depend on SQLite, gRPC, Avalonia, or OS APIs.

### 7.2 Application

Contains:

- import use cases;
- ZIP import orchestration;
- duplicate decision flow;
- search and filtering use cases;
- export use case;
- delete-to-trash use case;
- library verification use case;
- progress and cancellation orchestration.

Application depends on Domain abstractions only.

### 7.3 Infrastructure

Contains concrete implementations:

- SQLite repositories;
- EPUB parser;
- FB2 parser;
- ZIP reader;
- converter runner;
- managed file storage;
- cover storage;
- trash service implementation;
- Unicode/path helpers;
- logging;
- named-pipe host transport.

### 7.4 Presentation

Two presentation edges exist:

Core-side presentation:

- protobuf service adapter and pipe request dispatch;
- DTO mapping;
- transport validation;
- request/reply progress polling.

UI-side presentation:

- Avalonia views;
- ViewModels;
- commands;
- drag-and-drop adapters;
- job queue visualization;
- setup wizard.

## 8. Repository Layout

Recommended repository structure:

- `libs/` for native static libraries grouped by logical slice;
- `apps/` for executables and UI applications;
- `proto/` for shared protobuf contracts;
- `tests/` for unit, integration, and contract tests;
- `docs/` for repository-level documentation.

Rules:

- each native logical slice should have its own folder under `libs/<SliceName>/`;
- each native library folder should contain its own `CMakeLists.txt`;
- in native libraries, headers and implementation files should stay together in the same directory unless a different layout is clearly justified;
- application targets should depend on these slice libraries instead of accumulating all code in one large target.

## 9. Interop Strategy

### Recommended model

- Protobuf for DTOs and contracts;
- transport-neutral service adapters over protobuf request/response messages;
- named pipes as the Windows transport in the initial release scope;
- transport-level client or framing failures must be isolated to the current pipe session and must not terminate the whole host process;
- client-side pipe calls must use a bounded RPC timeout instead of waiting indefinitely for a response;
- cancellation propagated from UI to core job engine.

### Why not direct P/Invoke

Direct native interop would reduce startup complexity but creates tighter coupling around:

- memory ownership;
- ABI stability;
- async callbacks;
- Unicode marshalling;
- versioning.

Because future portability matters, a service boundary is the more stable choice.

### Why not mandatory gRPC runtime in the initial release scope

`gRPC` remains a valid future transport option, but it is not required for the initial release runtime boundary.

Reasons:

- the contract value comes from `Protobuf`, not specifically from `gRPC`;
- current operations are unary request/reply and job-poll oriented, so streaming is not required for the first release;
- Windows named pipes are the real transport requirement for the initial release scope;
- avoiding a mandatory `gRPC C++` runtime keeps the toolchain simpler and more stable on the current Windows stack.

## 10. Domain Model

### 10.1 Main aggregate

`Book` is the aggregate root.

It owns:

- `BookId`;
- normalized metadata;
- preferred stored format;
- managed file information;
- cover path;
- hash;
- timestamps.

### 10.2 Core value objects

```cpp
struct SBookId
{
    std::int64_t Value;
};

enum class EBookFormat
{
    Epub,
    Fb2
};

struct SBookMetadata
{
    std::string TitleUtf8;
    std::vector<std::string> AuthorsUtf8;
    std::string Language;
    std::optional<std::string> SeriesUtf8;
    std::optional<double> SeriesIndex;
    std::optional<std::string> PublisherUtf8;
    std::optional<int> Year;
    std::optional<std::string> Isbn;
    std::vector<std::string> TagsUtf8;
    std::optional<std::string> DescriptionUtf8;
    std::optional<std::string> Identifier;
};

struct SBookFileInfo
{
    EBookFormat Format;
    std::filesystem::path ManagedPath;
    std::uintmax_t SizeBytes;
    std::string Sha256Hex;
};

struct SBook
{
    SBookId Id;
    SBookMetadata Metadata;
    SBookFileInfo File;
    std::optional<std::filesystem::path> CoverPath;
    std::chrono::system_clock::time_point AddedAtUtc;
};
```

### 10.3 Duplicate classification

```cpp
enum class EDuplicateSeverity
{
    None,
    Probable,
    Strict
};

enum class EDuplicateReason
{
    SameHash,
    SameIsbn,
    SameNormalizedTitleAndAuthors
};

struct SDuplicateMatch
{
    EDuplicateSeverity Severity;
    EDuplicateReason Reason;
    SBookId ExistingBookId;
};
```

## 11. Key Interfaces

```cpp
class IBookRepository
{
public:
    virtual ~IBookRepository() = default;
    virtual SBookId Add(const SBook& book) = 0;
    virtual std::optional<SBook> GetById(SBookId id) const = 0;
    virtual void Remove(SBookId id) = 0;
};

class IBookQueryRepository
{
public:
    virtual ~IBookQueryRepository() = default;
    virtual std::vector<SBook> Search(const SSearchQuery& query) const = 0;
    virtual std::vector<SDuplicateMatch> FindDuplicates(const SCandidateBook& candidate) const = 0;
};

class IBookParser
{
public:
    virtual ~IBookParser() = default;
    virtual bool CanParse(EBookFormat format) const = 0;
    virtual SParsedBook Parse(const std::filesystem::path& filePath) const = 0;
};

class IBookConverter
{
public:
    virtual ~IBookConverter() = default;
    virtual bool CanConvert(EBookFormat from, EBookFormat to) const = 0;
    virtual SConversionResult Convert(const SConversionRequest& request, IProgressSink& progress, std::stop_token stopToken) const = 0;
};

class IManagedStorage
{
public:
    virtual ~IManagedStorage() = default;
    virtual SPreparedStorage PrepareImport(const SStoragePlan& plan) = 0;
    virtual void CommitImport(const SPreparedStorage& prepared) = 0;
    virtual void RollbackImport(const SPreparedStorage& prepared) noexcept = 0;
};

class ITrashService
{
public:
    virtual ~ITrashService() = default;
    virtual void MoveToTrash(const std::filesystem::path& path) = 0;
};

class ICoverProvider
{
public:
    virtual ~ICoverProvider() = default;
    virtual std::optional<SCoverData> TryResolve(const SBookMetadata& metadata) const = 0;
};
```

Notes:

- `ICoverProvider` exists from day one, but the default implementation is a null object;
- `IBookConverter` is an abstraction over any external conversion engine, not one concrete tool;
- `ITrashService` is cross-platform by contract, Windows-specific in the current implementation.

## 12. Import, Conversion, And Job Model

### 12.1 Single-file import flow

1. UI submits source path and import preferences.
2. Core detects format.
3. Parser extracts metadata and cover.
4. Duplicate classifier evaluates strict and probable matches.
5. Strict duplicate rejects import.
6. Probable duplicate returns a decision-required state.
7. If conversion is needed and available, converter runs.
8. If converter is unavailable or fails, original FB2 is stored with warning.
9. Managed storage stages files in a temporary area.
10. DB transaction writes book, authors, tags, and search index.
11. Storage commit finalizes managed files.
12. Job reports completion.

### 12.2 ZIP import flow

1. ZIP reader enumerates entries.
2. Supported files are extracted to temp workspace.
3. Each entry uses the same single-file pipeline.
4. Results are aggregated.
5. Failures do not roll back successful entries.

### 12.3 Job engine

Long-running operations must be modeled as jobs:

- import;
- bulk import;
- ZIP import;
- export;
- verification;
- conversion as part of import.

Each job supports:

- progress updates;
- warnings;
- structured failures;
- cancellation;
- final summary.

This is a better fit than blocking RPC methods.

## 13. Crash Safety And Rollback

Crash-safe import is required.

Strategy:

- files are first copied into a staging area under managed storage;
- database writes happen in a transaction;
- final commit happens only after the DB transaction is ready;
- failed imports roll back staged files;
- stale temp state is cleaned at startup by a recovery pass.

This is required for correctness during bulk import and conversion-heavy workflows.

## 14. File Storage Design

### Recommended layout

```text
LibraryRoot/
  Database/
    librova.db
  Books/
    0000000001/
      book.epub
  Covers/
    0000000001.jpg
  Temp/
  Logs/
```

### Why ID-based storage

Using `BookId`-based storage is preferable to `{Author}/{Title}` because it:

- keeps paths stable;
- avoids rename churn if metadata changes later;
- simplifies rollback;
- simplifies duplicate handling;
- supports future multi-format evolution;
- simplifies integrity verification.

Human-readable names can still be generated for export.

## 15. Database Design

Recommended logical schema:

- `books`
- `authors`
- `book_authors`
- `tags`
- `book_tags`
- `formats`
- `search_index`

Notes:

- `formats` is future-friendly even though the initial release scope stores one file per book;
- `books` stores normalized metadata and references to managed file and cover;
- author and tag normalization should be enforced at repository level;
- search should combine indexed relational filters and FTS-backed text lookup.

## 16. Search Design

Search should use a hybrid model:

- relational filters and sorting for structured data;
- FTS5 for title, author text, tags, and description;
- explicit normalization step for case folding and Cyrillic `e/yo` equivalence before indexing and query execution.

This avoids overloading FTS with responsibilities better handled by the relational model.

## 17. Unicode Strategy

Unicode must be treated as a first-class concern.

Rules:

- core strings use UTF-8;
- protobuf strings use UTF-8;
- C# uses standard .NET strings at the UI edge;
- metadata normalization and duplicate logic operate on normalized Unicode text;
- file path conversion is centralized;
- locale-dependent ANSI conversions are forbidden.

Windows path guidance:

- store metadata as UTF-8;
- convert to wide-character paths only at OS boundaries;
- keep path helper logic isolated in Infrastructure.

## 18. Service Contract Shape

The `.proto` contract should separate query, command, and long-running job concerns.

Recommended service groups:

- `LibraryQueryService`
- `LibraryCommandService`
- `LibraryJobService`

Recommended operation set:

- `GetLibraryOverview`
- `SearchBooks`
- `GetBookDetails`
- `StartImport`
- `ResolveProbableDuplicate`
- `ExportBook`
- `DeleteBook`
- `VerifyLibrary`
- `CancelJob`

For the initial release runtime transport, these operations may be carried over named-pipe request/reply envelopes instead of a full `gRPC` server.

The contract should return structured error codes and user-safe messages, not raw exception text.

## 19. UI Architecture

Recommended UI composition:

- `ShellViewModel`
- `LibraryViewModel`
- `SearchPanelViewModel`
- `BookGridViewModel`
- `BookDetailsPanelViewModel`
- `ImportJobsViewModel`
- `SetupWizardViewModel`

Recommended UI behavior:

- shell starts core process or connects to it through the named-pipe transport;
- setup wizard runs on first launch;
- library grid loads summary cards, not full book details;
- details panel loads on selection;
- jobs panel displays progress, warnings, and cancel actions;
- drag-and-drop feeds the same import command path as file picker import.

## 20. Extensibility Points

The architecture should allow future growth without rewriting the core use cases.

Key extension seams:

- `IBookParser` for new file formats;
- `IBookConverter` for different conversion backends;
- `ICoverProvider` for external cover/metadata enrichment;
- `ITrashService` for platform-specific deletion behavior;
- `ILibraryIntegrityService` for advanced repair tools;
- future collection subsystem as separate domain slice;
- future multi-format storage by expanding aggregate and `formats` usage.

## 21. Error Model

Core errors should be classified into explicit categories:

- validation error;
- unsupported format;
- duplicate rejected;
- probable duplicate decision required;
- parser failure;
- converter unavailable;
- converter failed;
- storage failure;
- database failure;
- cancellation;
- integrity issue.

This allows the UI to react correctly instead of parsing strings.

## 22. First-Run Wizard

The setup wizard should configure:

- library root path;
- converter executable or command template;
- default FB2 import policy;
- optional converter validation.

Recommended default FB2 policy:

- convert to EPUB when converter is available;
- otherwise store original FB2 and report warning.

### Converter configuration direction

The converter configuration should support two modes from the start:

- a built-in `fb2cng` profile for the default Windows setup;
- a user-defined external converter command expressed through an explicit argument-template contract.

Why this is preferred:

- `fb2cng` is a strong default for `FB2 -> EPUB`;
- the project should not be locked to one executable forever;
- argument-template configuration is simpler and more transparent than inventing a scripting DSL for the initial release scope;
- the setup wizard can validate one explicit executable path and one explicit command shape.

## 23. Testing Strategy

### 23.1 C++ unit tests with Catch2

Target:

- value objects;
- metadata validation;
- Unicode normalization;
- duplicate classification;
- path and storage planning;
- converter command building;
- query DTO validation.

### 23.2 C++ integration tests

Target:

- SQLite repositories;
- parser implementations;
- ZIP import;
- managed storage commit/rollback;
- converter runner with fake executable;
- library verification logic.

Representative scenarios:

- valid EPUB import writes DB rows and managed files;
- DB failure rolls back staged files;
- ZIP import returns partial success with per-entry errors;
- Cyrillic search works case-insensitively;
- deletion uses trash abstraction.

### 23.3 IPC contract tests

Target:

- request dispatch;
- protobuf serialization round-trips;
- cancellation;
- error mapping;
- duplicate decision flow;
- contract compatibility expectations.

### 23.4 UI tests

Recommended approach:

- `xUnit` for ViewModel tests;
- UI automation later if needed.

Target:

- setup wizard validation;
- job queue state transitions;
- details panel loading;
- drag-and-drop routing;
- command orchestration.

## 24. Development Roadmap

### Phase 0. Build Foundation

- repo structure with `libs/` for native static libraries, `apps/` for executables and UI, `proto/` for shared contracts, and `tests/`;
- each native logical slice as its own static library with a local `CMakeLists.txt`;
- CMake and dependency wiring;
- Avalonia solution bootstrap;
- Catch2 and UI test setup;
- local build and test entry points.

### Phase 1. Domain Core

- value objects;
- duplicate model;
- metadata normalization;
- domain errors;
- port interfaces.

### Phase 2. Persistence And Managed Storage

- SQLite schema;
- repositories;
- storage staging and rollback;
- trash abstraction.

### Phase 3. Parsers

- EPUB parser;
- FB2 parser;
- cover extraction;
- parser registry/factory.

### Phase 4. Search Layer

- relational filters;
- sorting;
- FTS integration;
- Unicode normalization for search.

### Phase 5. Converter Integration

- converter abstraction;
- external process runner;
- fallback behavior;
- cancellation support.

### Phase 6. Use Cases And Job Engine

- import workflows;
- ZIP import;
- export;
- delete to trash;
- verification;
- progress and cancellation orchestration.

### Phase 7. IPC Surface

- protobuf contracts;
- protobuf service adapters;
- named-pipe transport;
- generated C# client layer.

### Phase 8. UI Shell

- setup wizard;
- library grid;
- search/filter panel;
- jobs panel;
- right-side details panel.

### Phase 9. Stabilization

- drag-and-drop polish;
- verification UI;
- logging polish;
- recovery hardening;
- portable packaging.

## 25. Risks And Mitigations

### Unicode issues at boundaries

Mitigation:

- UTF-8 in core and transport;
- centralized path helpers;
- explicit Cyrillic fixtures and tests early.

### Converter variability

Mitigation:

- abstract command template;
- fake-converter tests;
- explicit fallback behavior.

### IPC complexity

Mitigation:

- narrow job-oriented contract;
- contract tests before full UI integration;
- DTOs independent from internal domain types;
- transport-neutral protobuf service adapters below the named-pipe host.

### Search correctness

Mitigation:

- hybrid relational + FTS design;
- explicit normalization layer;
- separate tests for filters and text search.

### Crash recovery gaps

Mitigation:

- staged import only;
- startup cleanup/recovery;
- rollback tests for storage-sensitive use cases.

## 26. Final Recommendations

The recommended architecture for Librova is:

- `C++20` native core;
- `Avalonia` UI on C#;
- separate backend process;
- `Protobuf` contract with transport-neutral service adapters;
- `SQLite` local persistence;
- ID-based managed storage;
- hybrid filtering + FTS search;
- explicit Unicode and Cyrillic normalization;
- TDD-first implementation with `Catch2`;
- job-oriented execution model for all long-running operations.

This architecture is intentionally conservative where correctness and maintainability matter, and flexible where future platform support and feature growth are likely.

## 27. Frozen Decisions

This section records decisions that should be treated as fixed unless explicitly revised later.

### Product scope

- The initial release target platform is Windows only.
- The initial release scope supports one user and one managed library only.
- Librova is a library manager, not a reading application.
- Metadata editing is out of scope for the initial release scope.
- Collections, favorites, shelves, and ratings are out of scope for the initial release scope, but future support must remain possible.

### UI

- UI framework is Avalonia.
- UI language is English only in the initial release scope.
- Main browsing mode includes a cover grid.
- Book details open in a right-side panel.
- Drag-and-drop import is required.
- Long operations must show progress and support cancellation.
- First-run setup wizard is required.

### Backend and interop

- Core is implemented in modern C++ without C++ Modules.
- `C++20` is the baseline standard.
- `C++/CLI` is forbidden.
- Architecture uses a separate native core process.
- UI and core communicate through `Protobuf` messages over a transport boundary.
- Windows transport in the initial release scope is named pipes.
- `gRPC` is not part of the required initial release runtime stack.

### Storage and import rules

- Imported files are always copied into managed storage.
- The initial release scope stores exactly one managed file per logical book.
- Preferred stored format is `EPUB` when available.
- If `FB2` conversion is unavailable or fails, the original `FB2` is stored with a warning.
- ZIP archives may contain folders, but not nested archives.
- ZIP import is partial-success, not all-or-nothing.
- Delete operation moves items to trash, not permanent deletion.

### Duplicate rules

- Same file hash is a strict duplicate.
- Same ISBN is a strict duplicate when ISBN exists.
- Same normalized title plus authors is a probable duplicate.
- Strict duplicates are rejected.
- Probable duplicates require explicit user confirmation.

### Persistence and search

- Local database is SQLite.
- Authors are normalized in storage.
- Cover is stored as a file on disk, not in the DB.
- Managed storage uses stable `BookId`-based folders, not `{Author}/{Title}` folders.
- Search must support case-insensitive Cyrillic matching.
- Cyrillic `e` and `yo` are treated as equivalent in search.
- Rich filtering is more important than search-only behavior.

### Testing and development approach

- Core development follows TDD-first.
- C++ unit tests use Catch2.
- IPC integration tests are required.
- Portable folder deployment is sufficient for the initial release scope.
- Local build and local tests are enough for the initial roadmap; CI/CD is not required yet.

## 28. Working Agreements

The architecture document is not the only project-level source of truth. The following companion documents are part of the repository bootstrap and are read automatically by Codex at the start of each session:

- `AGENTS.md`
- `docs/engineering/CodeStyleGuidelines.md`
- `docs/engineering/CommitMessageGuidelines.md`

Their purpose is:

- architecture document: product and technical decisions;
- code style document: implementation and naming conventions;
- commit message document: git history conventions;
- session instructions: startup checklist and task execution discipline.

If implementation reality changes any of these assumptions, update the relevant document instead of letting the repository drift.
