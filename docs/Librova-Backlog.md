# Librova Backlog

## 1. Working Model

Use a single backlog for active work. For all backlog operations (adding, closing, validating tasks) follow the `$backlog-update` skill.

Each task has four required fields plus one optional field in this order:

```
- `#<id>` <summary>
  - Status: `Open | Needs Reproduction | Blocked | Closed`
  - Type: `Feature | Bug`
  - Milestone: `<version>`
  - Note: <context or one-sentence completion summary>
```

Priority sections: `Critical` → `Major` → `Minor` → `Low`

`Milestone` is optional. When present, use a release string such as `1.0`, `1.1`, or the literal `unscheduled`.

Last assigned id: `#134`

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
- `#130` replace indeterminate import progress bar with a finite percentage counter
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: the import pipeline knows the total source-file count at job start; replace the infinite spinner/indeterminate bar with a deterministic progress bar and a live "N / Total — X%" label so the user can see how far along the job is; the counter must update in real time and be accurate for single-file, directory, and ZIP batch imports; the indeterminate animation may still be used briefly during the pre-scan phase before the total is known.

- `#132` disable and dim all shell regions outside the active import card during an import
  - Status: `Open`
  - Type: `Bug`
  - Milestone: `1.1`
  - Note: during an active import the left-panel logo + "Your shelf your story" tagline and the Ingest tab header controls ("Ingest", "Import Books", "Drop local…") remain visually bright and interactive; introduce a universal import-lock-out state in the shell that dims and disables every region outside the running import card; the mechanism should be generic enough to cover future long-running jobs without per-widget wiring.

- `#117` investigate why `Run-Tests.ps1` hangs when invoked from an automation/agent context.
  - Status: `Open`
  - Type: `Bug`
  - Milestone: `unscheduled`
  - Note: the script completes normally when run interactively but hangs indefinitely when called by the agent (cmake/ctest/dotnet output appears to stall). Likely cause: output buffering or an interactive prompt (e.g. developer-certificate trust dialog) not visible in automation. Needs a minimal repro and a non-interactive invocation fix or a `--no-interactive` wrapper.

- `#100` add `RAR` archive import support alongside the existing ZIP archive workflow.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: extend source selection, recursive directory discovery, and archive import handling so `.rar` files are accepted anywhere `.zip` is currently supported, with the same per-entry diagnostics, partial-success behavior, cancellation semantics, and duplicate handling as ZIP imports.

- `#128` add two explicit cancellation modes to the import dialog: "Cancel and discard" and "Stop here".
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: currently a single Cancel button stops the pipeline with undefined semantics regarding already-imported books; replace it with two explicit actions — "Cancel and discard" rolls back and removes all books imported in the current batch, "Stop here" halts new processing but retains every book successfully imported so far; both buttons must be disabled and replaced with a "Cancelling…" indicator while the pipeline winds down, so the user cannot submit duplicate requests.

- `#68` add a Batch Convert to EPUB workflow that re-converts all managed FB2 books to EPUB using the active converter, with aggregate progress and summary.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: the scenario is a user who already has an FB2 library and then configures a converter for the first time; a dedicated action (e.g., in Settings or the Library section) triggers conversion of every managed FB2 book; progress must surface the same aggregate counters as batch import (total, converted, failed, skipped); a per-book conversion failure must keep the original FB2 intact as the managed file — never remove it; the terminal summary must remain visible after the job finishes; cancellation must stop new conversions and leave already-converted books in their new EPUB state; the feature spans a new native pipeline stage, IPC contract, and UI workflow.

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
  - Note: remaining performance scope after remediation-wave hardening is keyset pagination for deep browse, scalable author-sort/index strategy, FTS5 query-plan/tokenizer review, composite indexes aligned with browse filters, and explicit response-time budgets for browse/search/filter at 100k and 500k book scales.

- `#62` introduce platform service interfaces for all Windows-specific operations so non-Windows paths throw explicitly and future ports have a clear, discoverable integration surface.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: affected areas span the IPC transport (named pipes), the Recycle Bin service, the cover image processor (WIC/COM), and the UI DWM title-bar integration; each should be hidden behind an interface with a Windows-only implementation in a dedicated platform layer; non-Windows stubs must throw explicitly rather than fail silently or refuse to compile; where a portable third-party implementation is straightforward (cross-platform image processing library, socket-based IPC) prefer it over a stub; the existing Unicode conversion guard is the reference pattern.

### Minor
- `#131` replace import-stage sub-label with rotating book facts
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: the import card currently shows the internal processing stage for the current book (e.g. "Parsing metadata"); replace this sub-label with a rotating display picked from a curated set of ~100 short book-related facts (one sentence, fits the card width); shuffle the set per session, show each fact for a few seconds then advance; the fact display must be hidden once the import completes or is cancelled.

- `#126` disable the Cancel button and show a "Cancelling…" indicator after the user clicks it during import.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: once a cancellation request is submitted, the Cancel button (or both cancel buttons after `#128` lands) must become disabled and the progress area must display a visible "Cancelling…" label or spinner so the user has unambiguous feedback that wind-down is in progress and cannot submit duplicate cancel requests.

- `#127` add an "Import covers" checkbox to the import dialog, checked by default.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: when unchecked, the import pipeline must skip cover extraction and storage entirely for the whole batch — no cover file is written to disk and no cover reference is stored in the database; this reduces disk usage and import time for users who do not need cover art; the setting applies only to the current import session and is not persisted globally.

- `#111` add clearer visual confirmation in `Settings` when a converter is configured successfully.
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: the converter settings panel currently lacks strong positive feedback after a valid converter path is accepted; add an explicit configured-state affordance such as a bright checkmark, stronger success styling, or a more prominent state label so the user can tell at a glance that conversion is enabled.

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
- `#129` compact database after regular book deletions to reclaim FTS5 tombstone space
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `unscheduled`
  - Note: every `Remove()` call inserts an FTS5 tombstone that is never cleaned up unless `Compact()` is called; regular UI-driven deletions via `LibraryTrashFacade` never trigger compaction, so tombstones accumulate indefinitely (~1–2 KB per deleted book); decide on a strategy (threshold-based, background idle task, or per-batch) and implement it so FTS5 shadow-table space is reclaimed without blocking the UI.

- `#122` audit and consolidate FB2 genre display names to reduce visual clutter in genre filters
  - Status: `Open`
  - Type: `Feature`
  - Milestone: `unscheduled`
  - Note: review the full genre mapping table and identify clusters of codes that resolve to overly similar or redundant display names (e.g. multiple regional sub-genres collapsing to one concept); consolidate them into a smaller, cleaner set of user-visible names without breaking import compatibility — code-to-name resolution must remain stable for all known FB2 codes.

- `#112` reduce native test-run log noise so routine runs show failures and final summary rather than every passing test case.
  - Status: `Open`
  - Type: `Bug`
  - Milestone: `1.1`
  - Note: the current C++ test invocation logs each individual passing test, which makes routine runs noisy and harder to scan; change the default test runner output to focus on failures and the final aggregate result while still keeping enough detail for debugging failed cases.

## 4. Done Criteria

A task can be considered closed only when:

- the behavior is actually changed in code;
- appropriate automated tests are added where practical;
- documentation is updated if user-visible behavior or working rules changed;
- the user confirms manual verification when the task comes from UI or manual-test remarks.
