---
name: sqlite
description: SQLite and SQL guidance for Librova. Use when changing the SQLite schema, writing or reviewing SQL in the native C++ persistence layer, debugging FTS5/search behavior, or auditing query safety and performance against the current Librova database design.
---

# SQLite For Librova

## Goal

Keep SQLite changes aligned with Librova's actual architecture, schema, and failure model.

This skill is intentionally narrow. It covers the native C++ persistence layer that owns the database. It does not try to be a general SQL tutorial.

## When To Use

- use this skill when editing SQL under `libs/Database/`
- use this skill when reviewing schema changes, FTS5 changes, duplicate-detection queries, or query-shape changes
- use this skill when debugging search quality, SQLite locking, or rollback behavior
- use this skill together with `$code-style` for C++ naming and structure rules
- do **not** use this skill for Avalonia / C# UI work that does not cross into the native persistence layer

## First Orientation

Read only the files relevant to the change:

- `docs/CodebaseMap.md`:
  - §3 Persistence
  - §7 Storage & Persistence Model
  - §8 Critical Invariants
  - §12 `IBookRepository` / `IBookQueryRepository`
  - §14.3 Storage Design
- `libs/Database/DatabaseSchema.cpp` for the current schema
- `libs/Database/SchemaMigrator.cpp` for version policy
- `libs/Database/SqliteBookRepository.cpp` for write-side patterns
- `libs/Database/SqliteBookQueryRepository.cpp` for read-side query patterns
- `libs/Database/SearchIndexMaintenance.cpp` for FTS maintenance
- `tests/Unit/TestDatabaseSchema.cpp`, `tests/Unit/TestSchemaMigrator.cpp`, and the relevant `TestSqlite*.cpp` coverage

## Non-Negotiable Librova Rules

### Ownership boundary

- SQLite belongs to the native C++ core
- `apps/Librova.UI/` must not access SQLite directly
- if a user-visible data need reaches the UI, expose it through the existing domain and IPC layers instead of adding managed-side SQL

### Use Librova wrappers first

- prefer `CSqliteConnection` over raw `sqlite3_open_v2`
- prefer `CSqliteStatement` over ad-hoc `sqlite3_prepare_v2` call sites
- only drop to raw SQLite APIs when the wrapper does not support the required feature yet, for example `sqlite3_progress_handler`

### Path and Unicode safety

- paths stored in SQLite are UTF-8 strings
- convert `std::filesystem::path` to DB text with `PathToUtf8()`
- convert DB text back to paths with `PathFromUtf8()`
- never construct `std::filesystem::path` directly from UTF-8 `std::string` values read from SQLite

### Schema version policy

- current schema version is `2`
- `user_version == 0` means a fresh database: create schema and set version to `2`
- `user_version == 1` is deliberately upgraded to `2` by adding collection infrastructure
- `user_version == 2` is a no-op
- any other version is treated as incompatible and must throw
- do not add automatic upgrade logic unless the decision is deliberate and non-destructive

## Current SQLite Design In Librova

### Connection defaults

`CSqliteConnection` currently applies these settings on every connection:

- `PRAGMA foreign_keys = ON;`
- `PRAGMA busy_timeout = 5000;`
- `PRAGMA cache_size = -32768;`

The write repository additionally sets:

- `PRAGMA wal_autocheckpoint = 4000;`

`journal_mode = WAL` is part of schema bootstrap / migration, not a generic per-query concern.

### Current schema shape

The schema is defined in `libs/Database/DatabaseSchema.cpp`.

Important tables:

- `books`
- `book_id_sequence`
- `authors` + `book_authors`
- `tags` + `book_tags`
- `genres` + `book_genres`
- `collections` + `book_collections`
- `search_index` as an FTS5 virtual table with `content=''`

Important `books` columns that frequently matter in code review:

- `normalized_title`
- `preferred_format`
- `storage_encoding`
- `managed_path`
- `cover_path`
- `file_size_bytes`
- `sha256_hex`
- `added_at_utc`

Do not write guidance against stale names such as `format`, `size_bytes`, or `added_at_unix_ms` when the code currently uses different columns.

### Search index model

- FTS table name is `search_index`
- it indexes `title`, `authors`, `tags`, `genres`, and `description`
- it is a contentless FTS5 table: maintenance is explicit
- write-side insert/remove flows call `CSearchIndexMaintenance`
- `RemoveBatch()` intentionally skips per-book FTS maintenance; the caller must later invoke `Compact()` to rebuild the index

## Query Patterns To Preserve

### General query-writing practices

These are generic in form, but they matter specifically for Librova's codebase:

- select only the columns the caller actually needs; avoid `SELECT *` in repository code
- treat `NULL` as part of the contract: decide explicitly whether a column is required, optional, or should be normalized before storage
- keep the SQL shape readable enough that bind order can be audited quickly during review
- prefer `EXISTS` for presence checks and per-book facet filters instead of accidental row multiplication through wide joins
- add `DISTINCT` only when duplicate rows are a real risk from the join shape; do not use it as a blind patch for unclear joins
- preserve deterministic ordering for user-visible lists and facet values; unstable ordering becomes UI jitter and test fragility
- keep read queries consistent with the stored representation: search and duplicate logic must use normalized fields where the schema already stores normalized data
- when a query becomes hard to reason about, split it into small local builder helpers instead of hiding complexity in one long string

### Bind parameters always

- never interpolate values into SQL strings
- dynamic SQL shape is acceptable for clause construction such as `IN (?, ?, ?)` or optional joins
- dynamic values still must be bound through `CSqliteStatement`

Librova wrappers already expose the common bind operations:

- `BindInt`
- `BindInt64`
- `BindDouble`
- `BindText`
- `BindNull`

Default to `BindInt64` for ids, counts stored as SQLite integers, and rowids.

### Normalize on the domain side, not in SQL

Librova already normalizes search-sensitive text before it hits SQLite:

- titles and free-text search use `Domain::NormalizeText(...)`
- ISBN matching uses `Domain::NormalizeIsbn(...)`
- duplicate detection and lookup queries depend on that normalized data being consistent

If you add a new search or duplicate heuristic, decide explicitly:

- what is stored normalized in SQLite
- what is preserved as display text
- what must be normalized at bind time

### FTS queries are sanitized before MATCH

User text is not passed straight into raw FTS syntax.

`CSqliteBookQueryRepository` builds the `MATCH` argument through `BuildFtsQuery(...)`:

- normalizes text
- strips punctuation into spaces
- preserves alnum and non-ASCII bytes
- emits quoted prefix tokens like `"толстой"*`

When changing search behavior:

- modify the builder and its tests, not only the SQL string
- keep punctuation-safe behavior for ordinary user input
- protect Cyrillic and other non-ASCII text

### Prefer explicit stable ordering

Existing browse queries use explicit tie-breakers:

- title sort: `normalized_title`, then `title`, then `id`
- author sort: derived first normalized author, then title tie-breakers
- date-added sort: `added_at_utc`, then title tie-breakers

When changing result ordering, preserve deterministic order across equal primary keys.

### Match current facet-query shape

Facet queries such as available languages, tags, and genres intentionally reuse the active filter set.

When editing those queries:

- keep text, author, series, tag, genre, language, and format filters aligned with the browse query where applicable
- prefer reusing the same bind order discipline as the surrounding helpers

## Write-Side Patterns To Preserve

### Write session lifecycle

`CSqliteBookRepository` keeps one SQLite write connection open for the repository lifetime and serializes operations with `m_operationMutex`.

Implications:

- do not add second concurrent write connections casually
- tests that delete the DB file must call `CloseSession()` first
- write operations should remain short and self-contained

### Transaction policy

Write operations use `BEGIN IMMEDIATE` through the local `CSqliteTransaction` RAII helper.

Preserve that pattern for:

- `Add`
- `ForceAdd`
- `ReserveId` / `ReserveIds`
- `Remove`
- `RemoveBatch`
- `AddBatch`

If a new write path is introduced, default to the same transaction model unless there is a very clear reason not to.

### Batch import behavior

`AddBatch(...)` is not just "many inserts":

- one outer transaction wraps the full batch
- each entry gets its own `SAVEPOINT`
- duplicate rejection rolls back only that entry
- other entry failures also roll back only that entry
- result count must still align with input count at higher layers

Do not replace this with independent per-book top-level transactions unless the calling import architecture changes deliberately.

### Search-index maintenance is explicit

Current maintenance model:

- `DoAddBook(...)` inserts the row and related tables, then updates `search_index`
- `Remove(...)` reads stored metadata, writes the FTS delete row, then deletes the book
- `Compact()` rebuilds the FTS index with live rows only, then runs `VACUUM`
- `OptimizeSearchIndex()` runs FTS `optimize` after bulk import

Do not add SQLite triggers for FTS maintenance on top of the current explicit call sites. That would create double-maintenance drift.

## Schema Change Checklist

When the schema changes, update all of these together:

1. `libs/Database/DatabaseSchema.cpp`
2. `libs/Database/SchemaMigrator.cpp` if version policy changes
3. repository/query code that reads or writes the changed columns
4. `tests/Unit/TestDatabaseSchema.cpp`
5. `tests/Unit/TestSchemaMigrator.cpp`
6. any affected repository/query tests
7. `docs/CodebaseMap.md` §7 or §12 if the implemented model changed

Before increasing `user_version`, make sure the project actually wants a migration path instead of the current recreate-on-mismatch policy.

## Review Checklist

Use this before closing a SQLite-related change:

- [ ] SQL lives in the native persistence layer, not the UI
- [ ] the query returns only the columns the caller truly needs
- [ ] all runtime values are bound parameters
- [ ] `NULL` handling is explicit and matches the domain contract
- [ ] ids and rowids use `BindInt64` / `GetColumnInt64` where appropriate
- [ ] UTF-8 path columns round-trip through `PathToUtf8()` and `PathFromUtf8()`
- [ ] text search behavior still goes through `BuildFtsQuery(...)` or an equivalent tested sanitizer
- [ ] joins, `EXISTS`, and `DISTINCT` reflect the intended row cardinality instead of masking duplicate-row bugs
- [ ] write-side changes preserve `BEGIN IMMEDIATE` and rollback semantics
- [ ] FTS maintenance remains consistent with the explicit `CSearchIndexMaintenance` model
- [ ] bulk delete / rollback flows still rebuild or compact FTS when required
- [ ] schema guidance and column names match the code that actually exists today
- [ ] tests cover the changed failure mode or query behavior

## Verification

Pick the smallest relevant set first, then expand if the change crossed boundaries:

- `dotnet test tests\\Librova.UI.Tests\\Librova.UI.Tests.csproj -c Debug` only if the managed contract changed
- `ctest --test-dir out\\build\\x64-debug -C Debug --output-on-failure` for native coverage
- targeted native tests are often the fastest signal:
  - `TestSqliteConnection`
  - `TestSchemaMigrator`
  - `TestDatabaseSchema`
  - `TestSqliteBookRepository`
  - `TestLibraryCatalogFacade`

If the change touched schema, search, duplicate detection, rollback, or path encoding, add or update a regression test instead of relying on manual inspection.
