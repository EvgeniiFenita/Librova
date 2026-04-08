# Librova Backlog

## 1. Working Model

Use a single backlog for active work. For all backlog operations (adding, closing, validating tasks) follow the `$backlog-update` skill.

Each task has four required fields in this order:

```
- `#<id>` <summary>
  - Status: `Open | Needs Reproduction | Blocked | Closed`
  - Type: `Feature | Bug`
  - Note: <context or one-sentence completion summary>
```

Priority sections: `Critical` → `Major` → `Minor` → `Low`

Last assigned id: `#99`

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
### Major
- `#67` add a Remove Converter button to Settings so the user can clear the configured converter in one action.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: the current Settings section requires the user to manually erase the converter path text field to disable conversion; add an explicit Remove button that clears the converter path and immediately reloads the session so that Export As EPUB and import conversion are hidden as they would be after a clean start with no converter configured.

- `#68` add a Batch Convert to EPUB workflow that re-converts all managed FB2 books to EPUB using the active converter, with aggregate progress and summary.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: the scenario is a user who already has an FB2 library and then configures a converter for the first time; a dedicated action (e.g., in Settings or the Library section) triggers conversion of every managed FB2 book; progress must surface the same aggregate counters as batch import (total, converted, failed, skipped); a per-book conversion failure must keep the original FB2 intact as the managed file — never remove it; the terminal summary must remain visible after the job finishes; cancellation must stop new conversions and leave already-converted books in their new EPUB state; the feature spans a new native pipeline stage, IPC contract, and UI workflow.

- `#26` complete first-class browser support for `series` and `genres` once metadata parsing and details display are in place.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: focus this item on browse-time behavior: filter sources, filter UI, request plumbing, result counts, and any related series/genres browsing flows rather than parser-only metadata extraction.

- `#27` complete release-candidate stabilization, diagnostics hardening, and manual verification.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.0`
  - Note: use this item for the remaining hardening pass instead of tracking stabilization in a separate standing-work section; startup now enforces explicit `Open Library` vs `Create Library` contracts, blocks silent in-place recreation for damaged libraries, keeps native CLI/logging Unicode-safe under Cyrillic library roots, keeps first-run bootstrap UI logs out of the chosen empty `Create Library` target until startup succeeds, uses explicit graceful host shutdown before any forced kill fallback, hardens free-text search against raw FTS punctuation input, and removes read-side `N+1` hydration from search plus probable-duplicate detection.

- `#59` add Favorites and Read as built-in user collections with per-book membership, sidebar navigation, filtered browse, card-level toggle controls, and details-panel membership display.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `unscheduled`
  - Note: each collection is a named list stored in the database; a book can independently belong to zero or more collections; the sidebar must surface Favorites and Read as first-class navigation destinations alongside Library; when a collection is selected the main grid shows only its books and all existing search, sort, and filter controls operate within that scope; book cards must carry icon toggles (star for Favorites, checkmark for Read) with tooltips and distinct visual states for added and not-added books; the details panel must list which collections the selected book belongs to; the change spans database schema, domain model, proto contracts, browse-request plumbing, and UI shell.

- `#60` introduce concurrent file processing in the import pipeline to reduce wall-clock import time for large batch, directory, and ZIP imports.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: the current import pipeline is fully sequential with no thread pool; introduce bounded task-based dispatch so multiple source files are processed concurrently; SQLite writes must be serialized through a single writer; duplicate detection must guard against concurrent insertion races (aligns with open task `#47`); the image-processing backend must be initialized per thread if it requires it; cancellation via the existing stop-token mechanism must propagate to all in-flight tasks; ZIP entry extraction may remain sequential while extracted entries feed the parallel pool; progress counters must be updated atomically and the UI aggregate summary must remain accurate under concurrent execution.

- `#61` audit and harden browse, search, and filter query performance so all key operations remain responsive at library scales of hundreds of thousands of books.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: the current `LIMIT/OFFSET` infinite-scroll model requires full index scans up to the current offset and degrades at large page depths — replace with keyset (cursor-based) pagination anchored on the active sort key and BookId; the author-sort path uses an uncached per-row subquery — replace with a materialized or indexed first-author-per-book approach; review FTS5 query plans and tokenizer configuration for large corpora; add composite indexes where sort key and filter predicates are used together; identify and eliminate any remaining full-library in-memory scans in duplicate detection and statistics paths; establish and document response-time budgets for browse, search, and filter at 100k and 500k book scales.

- `#62` introduce platform service interfaces for all Windows-specific operations so non-Windows paths throw explicitly and future ports have a clear, discoverable integration surface.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `unscheduled`
  - Note: affected areas span the IPC transport (named pipes), the Recycle Bin service, the cover image processor (WIC/COM), and the UI DWM title-bar integration; each should be hidden behind an interface with a Windows-only implementation in a dedicated platform layer; non-Windows stubs must throw explicitly rather than fail silently or refuse to compile; where a portable third-party implementation is straightforward (cross-platform image processing library, socket-based IPC) prefer it over a stub; the existing Unicode conversion guard is the reference pattern.

### Minor
- `#94` add missing regression coverage for damaged-library delete, duplicate-only batch import, forced conversion failure, and silent settings-state loss.
  - Status: `Open`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: add targeted tests for delete with missing managed file and cover, duplicate-only directory or ZIP imports, forced EPUB conversion that must fail instead of storing FB2, and `SavePreferencesAsync` preserving `Fb2CngConfigPath`.

- `#95` add the missing safety and lifecycle regressions for transport drift, rollback residue, portable containment, Unicode sort, and startup cleanup.
  - Status: `Open`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: cover the current blind spots around unknown proto enum rejection in the managed mappers, rollback residue surfacing after cancellation, portable-mode `..` containment, Cyrillic sort order, startup `Temp` symlink or junction escape, and converter shutdown timeout behavior.

- `#42` make compressed managed `FB2` storage visibly distinct from plain `.fb2` files inside the library.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: fallback-managed compressed `FB2` files currently keep the original `.fb2` extension even though their on-disk bytes are no longer plain `FB2`, which makes manual inspection of the managed library misleading; define and implement a clearer internal naming/layout rule for that storage representation.

- `#63` normalize FB2 genre codes to human-readable English names at import time so the database stores display-ready strings rather than raw format-specific codes.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: FB2 uses a fixed genre code vocabulary (e.g., sf_space, det_police, love_sf) with a well-known English-name mapping; a static code-to-name lookup table should live in the FB2 parser slice and be applied during metadata extraction before the book record is written; EPUB `dc:subject` values are already extracted as free-form strings and need basic normalization (trim, dedup, case); decide whether genres should remain unified with tags or become a distinct field across database schema, domain model, proto contracts, and details-panel display.

- `#64` allow the user to assign or replace a cover image for any managed book by selecting an image file from the filesystem, without modifying the source book file.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `unscheduled`
  - Note: the assigned image must pass through the existing cover processing pipeline (resize to the standard maximum dimensions and apply compression) before being stored in the managed library under the Covers directory; the UI affordance should appear in the details panel and open a native file picker filtered to common image formats; the operation must update the cover reference in the database and immediately refresh the card and details panel; it must also be possible to clear an assigned cover so the book reverts to its generated placeholder.

- `#65` convert all managed covers to JPEG at import time regardless of source format and replace the Windows-only WIC image-processing backend with a cross-platform library.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `unscheduled`
  - Note: the existing cover processor already resizes and applies JPEG encoding with a fixed quality value, but PNG source covers are stored as uncompressed PNG; extend the pipeline to always output JPEG so cover storage is consistently compact; replace the WIC/COM backend with a cross-platform library available through vcpkg (e.g., stb_image + stb_image_write or libjpeg-turbo) so this slice is no longer Windows-only, directly addressing the platform isolation goal of task `#62`; JPEG quality and maximum cover dimensions should be surfaced as configurable parameters rather than compile-time constants.

### Low
- `#96` validate the second-stage WinAPI string conversion calls in `UnicodeConversion` instead of assuming they cannot fail.
  - Status: `Open`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: `WideToUtf8` and `Utf8ToWide` check the sizing call but not the actual conversion call, which weakens the shared Unicode boundary the project relies on for Cyrillic-safe storage, logging, and path handling.

- `#97` replace named-pipe readable-byte busy polling with a more explicit wait strategy and cancellation-aware timeout handling.
  - Status: `Open`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: the current transport wait loop polls `PeekNamedPipe` with `sleep(5ms)`, which is functional but noisy and brittle as a transport primitive; tighten the behavior before 1.0 so IPC waiting semantics are clearer and easier to diagnose.

- `#98` remove the collision-path TOCTOU window in managed trash destination selection.
  - Status: `Open`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: `BuildCollisionSafePath` checks whether a candidate exists and then later renames into it, which leaves a small race window if another actor creates the same path between those operations.

- `#99` stop recreating default UI state and preferences stores on every file-path property read in `ShellViewModel`.
  - Status: `Open`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: the current `UiStateFilePath` and `UiPreferencesFilePath` accessors allocate new default stores on every read, which is low-risk but unnecessary churn in a shell diagnostics surface that should stay straightforward and deterministic.

## 4. Done Criteria

A task can be considered closed only when:

- the behavior is actually changed in code;
- appropriate automated tests are added where practical;
- documentation is updated if user-visible behavior or working rules changed;
- the user confirms manual verification when the task comes from UI or manual-test remarks.
