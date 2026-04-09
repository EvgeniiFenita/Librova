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

Last assigned id: `#102`

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
- `#102` rework import source selection so the picker can accept mixed files and folders in one action, matching drag-and-drop behavior.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: the current import UI forces the user to choose either files or a single folder even though drag-and-drop already accepts mixed combinations of files and folders; redesign the source-selection workflow so one import action can collect multiple folders, multiple files, or any mixture of both while preserving duplicate handling, progress reporting, and cancellation semantics.

- `#101` migrate the UI stack from Avalonia 11 to Avalonia 12 and validate the application against the Avalonia 12 breaking changes.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: upgrade all Avalonia package references together as one migration, resolve Avalonia 12 binding and API breaking changes, confirm that the `Tmds.DBus.Protocol` security warning is no longer present after the upgrade path, and rerun the full managed and manual UI verification pass for startup, library browsing, import flows, and shell navigation.

- `#100` add `RAR` archive import support alongside the existing ZIP archive workflow.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: extend source selection, recursive directory discovery, and archive import handling so `.rar` files are accepted anywhere `.zip` is currently supported, with the same per-entry diagnostics, partial-success behavior, cancellation semantics, and duplicate handling as ZIP imports.

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

- `#59` add Favorites and Read as built-in user collections with per-book membership, sidebar navigation, filtered browse, card-level toggle controls, and details-panel membership display.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
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
  - Milestone: `1.1`
  - Note: affected areas span the IPC transport (named pipes), the Recycle Bin service, the cover image processor (WIC/COM), and the UI DWM title-bar integration; each should be hidden behind an interface with a Windows-only implementation in a dedicated platform layer; non-Windows stubs must throw explicitly rather than fail silently or refuse to compile; where a portable third-party implementation is straightforward (cross-platform image processing library, socket-based IPC) prefer it over a stub; the existing Unicode conversion guard is the reference pattern.

### Minor
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
  - Milestone: `1.1`
  - Note: the assigned image must pass through the existing cover processing pipeline (resize to the standard maximum dimensions and apply compression) before being stored in the managed library under the Covers directory; the UI affordance should appear in the details panel and open a native file picker filtered to common image formats; the operation must update the cover reference in the database and immediately refresh the card and details panel; it must also be possible to clear an assigned cover so the book reverts to its generated placeholder.

- `#65` convert all managed covers to JPEG at import time regardless of source format and replace the Windows-only WIC image-processing backend with a cross-platform library.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: the existing cover processor already resizes and applies JPEG encoding with a fixed quality value, but PNG source covers are stored as uncompressed PNG; extend the pipeline to always output JPEG so cover storage is consistently compact; replace the WIC/COM backend with a cross-platform library available through vcpkg (e.g., stb_image + stb_image_write or libjpeg-turbo) so this slice is no longer Windows-only, directly addressing the platform isolation goal of task `#62`; JPEG quality and maximum cover dimensions should be surfaced as configurable parameters rather than compile-time constants.

### Low

## 4. Done Criteria

A task can be considered closed only when:

- the behavior is actually changed in code;
- appropriate automated tests are added where practical;
- documentation is updated if user-visible behavior or working rules changed;
- the user confirms manual verification when the task comes from UI or manual-test remarks.
