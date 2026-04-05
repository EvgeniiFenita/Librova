# Librova Backlog

## 1. Working Model

Use a single backlog for active work.

Each task has:

- `Priority`: `Critical`, `Major`, `Minor`, `Low`
- `Type`: `Feature`, `Bug`
- `Status`: `Open`, `Needs Reproduction`, `Blocked`, `Closed`

Working rule:

- if the user asks to "move to the next task", take the highest-priority task with status `Open`;
- if a higher-priority task is marked `Needs Reproduction` or `Blocked`, take the next open task;
- if several tasks share the same priority, prefer the most local and easiest-to-verify task first;
- when a task is completed, move it out of `Open Backlog` into the matching priority section under `Closed Backlog` in the same change;
- `Open Backlog` must never contain an item whose status is `Closed`.

Last assigned id: `#65`

Backlog edit rules:

- every task entry must be exactly four lines in this shape:
  `- #<id> <summary>`
  `  - Status: <status>`
  `  - Type: <type>`
  `  - Note: <note>`
- every new task must get a new unique id greater than `Last assigned id`; update `Last assigned id` in the same change;
- every new open task must be inserted only under `## 3. Open Backlog` in the matching priority section;
- when taking a task into work, do not duplicate, rename, or move it yet unless reprioritization is intentional; keep it in `Open Backlog` with status `Open`, `Needs Reproduction`, or `Blocked`;
- when closing a task, in the same change remove the full four-line entry from `Open Backlog`, set its status to `Closed`, replace its `Note` with a one-sentence summary of what was actually done (no exhaustive detail), and append the entry to the matching priority section in `docs/Librova-Backlog-Archive.md`;
- after any backlog edit, verify there are no orphan task-title lines without `Status`, `Type`, and `Note` in this file, and that no id present here also appears in the archive.

## 2. Priority Meanings

- `Critical`: breaks a core user flow or leaves the current release scope functionally incomplete.
- `Major`: a strong UX or layout defect that noticeably hurts day-to-day use.
- `Minor`: local polish or visual consistency work without a broken flow.
- `Low`: useful but least urgent polish.

## 2a. Type Meanings

- `Feature`: new capability or intentional improvement to existing behaviour.
- `Bug`: deviation from expected or previously working behaviour.

## 3. Open Backlog

### Critical
- `#44` store managed book and cover paths as stable library-relative values instead of absolute filesystem paths.
  - Status: `Open`
  - Type: `Bug`
  - Note: current import persistence writes absolute managed paths into SQLite, which breaks library portability and drifts from the transport/tests that expect `Books/...` and `Covers/...` relative paths; fix persistence, read-side resolution, and any affected transport mapping together.

- `#45` prevent managed-trash cleanup from deleting required top-level library directories such as `Covers`.
  - Status: `Open`
  - Type: `Bug`
  - Note: deleting the last cover currently allows post-move cleanup to remove `LibraryRoot/Covers`, which then causes existing-library bootstrap validation to fail on the next startup; keep cleanup below the protected top-level layout only.

- `#46` make staged delete crash-safe across filesystem moves and catalog removal.
  - Status: `Open`
  - Type: `Bug`
  - Note: the current delete flow moves managed files into library `Trash` before removing the database row, so a crash in between can leave a catalog entry pointing at a missing managed file; tighten ordering or add durable recovery semantics so delete remains consistent across process interruption.

- `#47` harden duplicate protection against concurrent imports with database-level enforcement.
  - Status: `Open`
  - Type: `Bug`
  - Note: duplicate detection currently relies on read-side `FindDuplicates` checks plus a non-unique `sha256_hex` index, which still allows parallel import races to insert the same book twice; add a schema-level uniqueness barrier and make the write path handle the conflict explicitly.

- `#48` compute and propagate SHA-256 for batch and ZIP imports so strict duplicate detection works beyond single-source manual requests.
  - Status: `Open`
  - Type: `Bug`
  - Note: the normal UI and multi-source import flow do not populate `sha256_hex`, so strict duplicate detection by content hash is effectively absent for batch and ZIP imports; compute the hash in the real import pipeline and cover it with regression tests.

### Major
- `#49` reject databases created by a newer schema version instead of silently downgrading `user_version`.
  - Status: `Open`
  - Type: `Bug`
  - Note: schema migration currently accepts forward-version databases and then unconditionally rewrites `PRAGMA user_version` to the local version, which risks running an older binary against an incompatible schema; fail fast with explicit recovery guidance.

- `#26` complete first-class browser support for `series` and `genres` once metadata parsing and details display are in place.
  - Status: `Open`
  - Type: `Feature`
  - Note: focus this item on browse-time behavior: filter sources, filter UI, request plumbing, result counts, and any related series/genres browsing flows rather than parser-only metadata extraction.

- `#27` complete release-candidate stabilization, diagnostics hardening, and manual verification.
  - Status: `Open`
  - Type: `Feature`
  - Note: use this item for the remaining hardening pass instead of tracking stabilization in a separate standing-work section; startup now enforces explicit `Open Library` vs `Create Library` contracts, blocks silent in-place recreation for damaged libraries, keeps native CLI/logging Unicode-safe under Cyrillic library roots, keeps first-run bootstrap UI logs out of the chosen empty `Create Library` target until startup succeeds, uses explicit graceful host shutdown before any forced kill fallback, hardens free-text search against raw FTS punctuation input, and removes read-side `N+1` hydration from search plus probable-duplicate detection.

- `#58` expose sort controls in the Library section and wire them to the existing backend sort implementation.
  - Status: `Open`
  - Type: `Feature`
  - Note: the backend already supports six sort keys (title, author, date added, series, publication year, file size) at the proto, domain, and SQL levels; the Library browser currently sends no sort preference and has no sort state; add a sort selector control to the Library section header, capture sort state in the browser ViewModel, include the selected sort key in every book-list request, reset the scroll position on sort change, and persist the last-used sort preference across sessions; the author-sort SQL path uses an uncached per-row subquery and should be tracked as a separate performance concern.

- `#59` add Favorites and Read as built-in user collections with per-book membership, sidebar navigation, filtered browse, card-level toggle controls, and details-panel membership display.
  - Status: `Open`
  - Type: `Feature`
  - Note: each collection is a named list stored in the database; a book can independently belong to zero or more collections; the sidebar must surface Favorites and Read as first-class navigation destinations alongside Library; when a collection is selected the main grid shows only its books and all existing search, sort, and filter controls operate within that scope; book cards must carry icon toggles (star for Favorites, checkmark for Read) with tooltips and distinct visual states for added and not-added books; the details panel must list which collections the selected book belongs to; the change spans database schema, domain model, proto contracts, browse-request plumbing, and UI shell.

- `#60` introduce concurrent file processing in the import pipeline to reduce wall-clock import time for large batch, directory, and ZIP imports.
  - Status: `Open`
  - Type: `Feature`
  - Note: the current import pipeline is fully sequential with no thread pool; introduce bounded task-based dispatch so multiple source files are processed concurrently; SQLite writes must be serialized through a single writer; duplicate detection must guard against concurrent insertion races (aligns with open task `#47`); the image-processing backend must be initialized per thread if it requires it; cancellation via the existing stop-token mechanism must propagate to all in-flight tasks; ZIP entry extraction may remain sequential while extracted entries feed the parallel pool; progress counters must be updated atomically and the UI aggregate summary must remain accurate under concurrent execution.

- `#61` audit and harden browse, search, and filter query performance so all key operations remain responsive at library scales of hundreds of thousands of books.
  - Status: `Open`
  - Type: `Feature`
  - Note: the current `LIMIT/OFFSET` infinite-scroll model requires full index scans up to the current offset and degrades at large page depths — replace with keyset (cursor-based) pagination anchored on the active sort key and BookId; the author-sort path uses an uncached per-row subquery — replace with a materialized or indexed first-author-per-book approach; review FTS5 query plans and tokenizer configuration for large corpora; add composite indexes where sort key and filter predicates are used together; identify and eliminate any remaining full-library in-memory scans in duplicate detection and statistics paths; establish and document response-time budgets for browse, search, and filter at 100k and 500k book scales.

- `#62` introduce platform service interfaces for all Windows-specific operations so non-Windows paths throw explicitly and future ports have a clear, discoverable integration surface.
  - Status: `Open`
  - Type: `Feature`
  - Note: affected areas span the IPC transport (named pipes), the Recycle Bin service, the cover image processor (WIC/COM), and the UI DWM title-bar integration; each should be hidden behind an interface with a Windows-only implementation in a dedicated platform layer; non-Windows stubs must throw explicitly rather than fail silently or refuse to compile; where a portable third-party implementation is straightforward (cross-platform image processing library, socket-based IPC) prefer it over a stub; the existing Unicode conversion guard is the reference pattern.

### Minor
- `#42` make compressed managed `FB2` storage visibly distinct from plain `.fb2` files inside the library.
  - Status: `Open`
  - Type: `Feature`
  - Note: fallback-managed compressed `FB2` files currently keep the original `.fb2` extension even though their on-disk bytes are no longer plain `FB2`, which makes manual inspection of the managed library misleading; define and implement a clearer internal naming/layout rule for that storage representation.

- `#63` normalize FB2 genre codes to human-readable English names at import time so the database stores display-ready strings rather than raw format-specific codes.
  - Status: `Open`
  - Type: `Feature`
  - Note: FB2 uses a fixed genre code vocabulary (e.g., sf_space, det_police, love_sf) with a well-known English-name mapping; a static code-to-name lookup table should live in the FB2 parser slice and be applied during metadata extraction before the book record is written; EPUB `dc:subject` values are already extracted as free-form strings and need basic normalization (trim, dedup, case); decide whether genres should remain unified with tags or become a distinct field across database schema, domain model, proto contracts, and details-panel display.

- `#64` allow the user to assign or replace a cover image for any managed book by selecting an image file from the filesystem, without modifying the source book file.
  - Status: `Open`
  - Type: `Feature`
  - Note: the assigned image must pass through the existing cover processing pipeline (resize to the standard maximum dimensions and apply compression) before being stored in the managed library under the Covers directory; the UI affordance should appear in the details panel and open a native file picker filtered to common image formats; the operation must update the cover reference in the database and immediately refresh the card and details panel; it must also be possible to clear an assigned cover so the book reverts to its generated placeholder.

- `#65` convert all managed covers to JPEG at import time regardless of source format and replace the Windows-only WIC image-processing backend with a cross-platform library.
  - Status: `Open`
  - Type: `Feature`
  - Note: the existing cover processor already resizes and applies JPEG encoding with a fixed quality value, but PNG source covers are stored as uncompressed PNG; extend the pipeline to always output JPEG so cover storage is consistently compact; replace the WIC/COM backend with a cross-platform library available through vcpkg (e.g., stb_image + stb_image_write or libjpeg-turbo) so this slice is no longer Windows-only, directly addressing the platform isolation goal of task `#62`; JPEG quality and maximum cover dimensions should be surfaced as configurable parameters rather than compile-time constants.

### Low
- `#43` show explicit busy or progress feedback during long-running `Export As EPUB` operations.
  - Status: `Open`
  - Type: `Feature`
  - Note: when `Export As EPUB` needs to decode a compressed managed `FB2` and then run conversion, the UI currently looks idle while no output file has appeared yet; add visible export-in-progress feedback such as disabled actions, wait cursor, status text, or progress so the running operation is obvious.

## 4. Done Criteria

A task can be considered closed only when:

- the behavior is actually changed in code;
- appropriate automated tests are added where practical;
- documentation is updated if user-visible behavior or working rules changed;
- the user confirms manual verification when the task comes from UI or manual-test remarks.
