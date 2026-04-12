# Librova Backlog — Archive

This file contains all closed backlog tasks.
It is not required reading during normal development — open tasks live in `Librova-Backlog.md`.
Consult this file when you need to look up past work, verify what was done, or confirm that a task id is not being reused.

## Closed Backlog

### Minor

- `#123` add diagnostic fields to duplicate-rejection log lines so actual match values are visible.
  - Status: `Closed`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: added `LogDuplicateCandidateDetails` helper in `SingleFileImportCoordinator.cpp`; every strict/probable duplicate rejection now emits a `[warning]` line with `title`, `authors`, `reason` (SameHash/SameIsbn/SameNormalizedTitleAndAuthors), `matchValue`, and `existingBookId` before returning the result.

- `#125` demote SameIsbn duplicate check from Strict to Probable.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.1`
  - Note: lib.rus.ec archives assign a single anthology ISBN to every story inside it; the Strict ISBN check caused 36 false-positive rejections in a real import run. Changed `AppendStrictDuplicateMatches` → `AppendDuplicateMatches` with an explicit severity parameter; SameIsbn now returns `EDuplicateSeverity::Probable`, prompting a user decision instead of auto-rejecting. SameHash remains Strict.


  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.1`
  - Note: all three codes observed in lib.rus.ec archives mapped to nothing and fell back to raw code display; added to `Fb2GenreMapper.cpp` (`sci_economy`/`economics` → "Economics", `management` → "Management") with corresponding `REQUIRE` assertions in `TestFb2GenreMapper.cpp`.

### Critical

- `#114` prevent native host startup from crashing before first-run library creation completes.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.1`
  - Note: Windows UTF-8 path parsing now converts through the shared UTF-16 helper before constructing `std::filesystem::path`, restoring stable core-host startup for `--help`, first-run create-library bootstrap, and other pipe/library-root argument paths.

- `#109` close the remaining pre-merge review blockers before merging the refactoring branch.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.1`
  - Note: removed the reintroduced managed mapper drift fallbacks, finished the safe catalog transport cleanup, restored mandatory IPC success-path logging, and aligned the regression surface, docs, and tests with the final lockstep UI+host transport contract.

- `#108` restore immediate first-run folder selection interaction after the architecture refactor.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: first-run folder picking now applies cheap validation immediately, runs mode-specific filesystem checks in the background with stale-result guards, and re-validates authoritatively on Continue so Browse and Continue stay responsive without accepting invalid library roots.

- `#86` block portable-mode library-root preferences from escaping outside the portable application directory.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: portable library-root preferences now ignore paths that escape the packaged app directory and no longer fall back to out-of-root saved absolute preferences in portable mode, with regressions for both preference building and startup-time resolution.

- `#85` make author and title sorting Unicode-aware for Cyrillic and other non-ASCII text.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: browse sorting now uses normalized title and author fields instead of SQLite `COLLATE NOCASE`, with regression coverage for Cyrillic title and author ordering.

- `#84` enforce forced FB2-to-EPUB conversion as a hard requirement instead of silently storing the original FB2 on converter failure.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: forced FB2-to-EPUB conversion now fails the import entry when the converter is unavailable or returns an error, with regression coverage at both conversion-policy and single-file importer levels.

- `#83` replace infinite waits in external converter termination and cancellation paths with bounded shutdown logic.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: external converter shutdown now uses bounded waits with structured fallback through the job object and a regression test that proves cancellation returns promptly instead of hanging behind converter sleep.

- `#82` preserve `Fb2CngConfigPath` when saving Settings preferences.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: Settings saves now preserve the stored fb2cng config path alongside the executable path, with a regression test covering the normal save flow.

- `#81` surface rollback cleanup residue when import cancellation leaves managed files or covers on disk.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: cancellation rollback now returns explicit residue warnings for managed artifacts left on disk and promotes that condition into the terminal cancelled message, with native and UI regression coverage.

- `#79` stop classifying duplicate-only batch and ZIP imports as `UnsupportedFormat`.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: batch and ZIP imports now preserve duplicate-only terminal semantics through an explicit no-success reason, so all-duplicate workloads fail as `DuplicateRejected` or `DuplicateDecisionRequired` instead of misleading `UnsupportedFormat`.

- `#78` make delete/trash resilient when managed book files or covers are already missing from the library.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: damaged-library deletes now treat missing managed book or cover files as best-effort cleanup residue, still remove the catalog row, and report `HasOrphanedFiles` with regression coverage for both missing-file cases.

- `#80` remove silent enum fallbacks from the C# IPC mappers and fail fast on contract drift.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: managed IPC mappers now throw explicit `InvalidOperationException` values for unexpected delete-destination, book-format, import-mode, and import-status enums, with regression tests covering the contract-drift cases.

- `#76` make portable installs reopen moved libraries and let first-run open an existing managed library.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: portable preferences now save a relocatable library-root hint for packaged runs, startup reopens the moved library from that relative path, and first-run setup can explicitly either create a new library or open an existing managed one.

- `#70` harden import cancellation rollback so partial rollback failures cannot leave the library in a half-removed state.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: cancellation rollback now removes catalog rows before best-effort file cleanup, preserves fully intact books when repository removal fails, and adds a regression test for repository failure during rollback.

- `#44` store managed book and cover paths as stable library-relative values instead of absolute filesystem paths.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: fixed import coordinator to write `RelativeBookPath`/`RelativeCoverPath` (computed via `std::filesystem::relative`) into `SBook` instead of absolute `FinalBookPath`/`FinalCoverPath`; consolidated duplicated `IsSafeRelativeManagedPath` helper into `ManagedPathSafety`; regression tests added.

- `#46` make staged delete crash-safe across filesystem moves and catalog removal.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: reversed delete ordering — catalog removal (DB) is now the commit point; file moves to Trash happen after and are best-effort with orphan logging on failure; regression tests updated.

- `#47` harden duplicate protection against concurrent imports with database-level enforcement.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: `Add()` now re-checks sha256_hex inside a `BEGIN IMMEDIATE` transaction and throws `CDuplicateHashException` on conflict; `ForceAdd()` bypasses the check for explicit duplicate overrides; coordinator catches the exception and either rejects or force-inserts based on `AllowProbableDuplicates`; `ForceAdd` made pure virtual to enforce explicit implementation in all mocks; double-warning guard added; regression tests cover conflict detection, force-add path, empty-hash no-throw, and rollback correctness.

- `#48` compute and propagate SHA-256 for batch and ZIP imports so strict duplicate detection works beyond single-source manual requests.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: added `libs/Hashing` slice with `ComputeFileSha256Hex` backed by Windows BCrypt API; `SingleFileImportCoordinator` now computes SHA-256 from the source file when the caller does not provide one, so all import paths (single-file, batch, ZIP) participate in hash-based duplicate detection; regression tests added for both the hash utility and the coordinator's auto-compute path.

- `#30` add a reproducible Windows portable release packaging flow that produces a fully runnable distribution folder for `Librova.UI` plus `Librova.Core.Host`, with every required runtime dependency either statically linked or copied into the output.
  - Status: `Closed`
  - Type: `Feature`
  - Note: `scripts/PublishPortable.ps1` is now the canonical Windows packaging flow. It publishes `Librova.UI` as a self-contained single-file `win-x64` executable, builds `Librova.Core.Host` through the `x64-release-static` preset so the host can ship as a single native `exe`, copies any remaining runtime dependencies into `out/package/win-x64-portable/`, and runs a portable smoke test before reporting success. The portable package now also keeps bootstrap UI state, preferences, and pre-library logs under `PortableData/` beside the executables instead of `%LOCALAPPDATA%`, while active library logs still move into `LibraryRoot/Logs`. The verified output currently contains `Librova.UI.exe`, `LibrovaCoreHostApp.exe`, and the Avalonia native runtime files `av_libglesv2.dll`, `libHarfBuzzSharp.dll`, and `libSkiaSharp.dll`.

- `#15` wire configured `fbc.exe` / `fb2cng` usage into import and export flows, with `Export As EPUB` and forced-import-conversion UI affordances enabled only when the converter path is configured for the current session.
  - Status: `Closed`
  - Type: `Feature`
  - Note: converter path is now read from preferences; Export As EPUB and forced-conversion options are gated on a non-empty path.

- `#7` show deterministic import progress for large folders, including total file count, processed count, and final outcome summary.
  - Status: `Closed`
  - Type: `Feature`
  - Note: import now reports total, processed, and skipped/failed counts via progress events; UI shows a deterministic summary on completion.

- `#33` harden `CoreHostProcess` command-line quoting so core startup survives Windows paths with trailing backslashes and embedded quotes.
  - Status: `Closed`
  - Type: `Bug`
  - Note: `CoreHostProcess` now escapes launch arguments with Windows-correct `CommandLineToArgvW` rules, and UI-side regression coverage verifies trailing `\`, embedded `"`, and Cyrillic paths through a real parse round-trip.

### Major

- `#118` harden FB2 parser against real-world lib.rus.ec quirks: non-numeric sequence numbers, non-integer years, missing lang, expand genre mapper, reduce import log noise.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: `TryParseNumber<T>` helper replaces `ParseExactNumber` for sequence and year — invalid values now warn+skip instead of throwing; missing `<lang>` node warns and falls back to empty string; 53 new genre codes added to `CFb2GenreMapper` (from two lib.rus.ec import batches); C++ polling methods (`GetImportJobSnapshot`, `WaitImportJob completed=false`) downgraded to Debug so they no longer appear in host.log; C# file sink restricted to `Information` level so `TryGetSnapshotAsync` and `WaitAsync` polling no longer appear in ui.log; 345/272 tests pass.

- `#115` improve FB2 parser encoding robustness: strip UTF-8 BOM, add CP1251 fallback, and recover metadata from files with malformed body XML.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: added `IsValidUtf8()` RFC 3629 state machine; `ReadTextFile()` strips BOM, converts explicit CP1251 declarations, falls back to CP1251 for misdeclared files; unknown FB2 genre codes emit `Warn`; added `TryParseDescriptionOnly()` fallback in `ParseXml()` — when strict pugixml parse fails (e.g. lib.rus.ec `<...>` ellipsis or `<:>` separators in body text), extracts `<description>` and parses it in isolation to recover metadata; `Warn` logged on recovery; cover extraction gracefully yields nothing when binary nodes absent; added `child_4` and `child_characters` genre codes to `Fb2GenreMapper`; all fixes covered by tests; 344/272 tests pass.

- `#116` separate genres from tags: introduce first-class `genres`/`book_genres` model with provenance and rebuild legacy libraries by re-parsing managed files.
  - Status: `Closed`
  - Type: `Feature`
  - Milestone: `1.0`
  - Note: implemented `genres`/`book_genres` schema (schema v1), `GenresUtf8` domain field separate from `TagsUtf8`; FB2 parser resolves raw genre codes to human-readable English via `CFb2GenreMapper`; EPUB parser stores `dc:subject` as-is; repository write/read sides, proto fields, C# models and mappers wired; genre filter and details panel display genres from the new model; FTS `search_index` gains a `genres` column; all pre-v1 DB migration infrastructure removed — incompatible databases require recreation; closes `#63`.

- `#63` normalize FB2 genre codes to human-readable English names at import time so the database stores display-ready strings rather than raw format-specific codes.
  - Status: `Closed`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: superseded by and closed as part of `#116`; FB2 genre codes are now resolved to human-readable English names at parse time via `CFb2GenreMapper` and stored in the `genres`/`book_genres` tables with `source_type = 'fb2_genre'`.

- `#26` complete first-class browser support for `genres` once metadata parsing and details display are in place.
  - Status: `Closed`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: закрыто в рамках #113 — жанровая фильтрация реализована полностью: proto repeated fields, SQL OR-семантика, FilterFacetItem VM, GenreFacets в LibraryBrowserViewModel, XAML Popup с multi-select; browse-time genre behavior полностью покрыт.

- `#113` redesign library filters into a single faceted panel with multi-select languages and genres.
  - Status: `Closed`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: реализован полный вертикальный срез — proto repeated fields (languages/genres), domain/application слои, SQL (OR-семантика для жанров, IN для языков), proto mapper, C# модели и маппер, FilterFacetItem VM, LibraryBrowseQueryState, LibraryBrowserViewModel (LanguageFacets/GenreFacets, IsFilterPanelOpen, ClearAllFiltersCommand), XAML ToggleButton + Popup, стили (FilterButton/FilterPopup/FilterSectionHeader), иконка IconFilter; тесты C++ и C# обновлены и расширены.

- `#110` fix the first-run screen so the `Librova` label in the left panel is not clipped vertically.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.1`
  - Note: the first-run left hero panel now gives the `Librova` wordmark an explicit 30 px line-height so Avalonia does not clip it vertically on the setup screen.

- `#107` implement and prove the intended reusable SQLite write-session lifecycle for the repository layer.
  - Status: `Closed`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: `CSqliteBookRepository` now keeps a reusable per-instance SQLite write session, and the native regression surface explicitly proves both the retained-session lifecycle and the required cleanup sequencing on Windows.

- `#106` finish the remaining remediation decomposition of import and browse orchestration after the first helper extractions.
  - Status: `Closed`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: import workload planning, rollback cleanup, and structured progress mapping now live in dedicated application helpers, while browse selection/details/export/delete flow moved behind a selection workflow controller so `LibraryImportFacade` and `LibraryBrowserViewModel` keep thinner composition-only boundaries without changing behavior.

- `#105` finish the storage-safe catalog transport cleanup so browse/details no longer expose raw managed storage paths and content hashes as their primary UI contract.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.1`
  - Note: browse/details transport now carries safe storage metadata (`managed_file_name`, `cover_file_extension`, cover/hash availability) while the native mapper stops populating raw managed paths and content hashes; the UI resolves cover resources locally from the safe descriptor and no longer depends on raw storage leakage as its production mapping contract.

- `#104` remove partial-construction semantics from `LibraryCatalogFacade` and `LibraryImportFacade` so every production-shaped instance is fully wired.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.1`
  - Note: removed the partial constructors and nullable repository/library-root state from both facades, updated the transport/job/catalog test surface to build only fully wired instances, and deleted the stale details-without-repository test that encoded the old semantics.

- `#103` finish the architecture remediation plan and align the backlog state with the remaining incomplete waves.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.1`
  - Note: fixed the branch hardening regressions that remained after review, restored honest backlog state, and split the still-large unfinished remediation waves into dedicated follow-up tasks instead of archiving them implicitly.

- `#102` rework import source selection so the picker can accept mixed files and folders in one action, matching drag-and-drop behavior.
  - Status: `Closed`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: decided not to implement; current two-button UX (Select Files... / Select Folder...) is considered sufficient.

- `#101` migrate the UI stack from Avalonia 11 to Avalonia 12 and validate the application against the Avalonia 12 breaking changes.
  - Status: `Closed`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: bumped Avalonia packages to 12.0.0, added `x:DataType` to all DataTemplates for compiled bindings, migrated drag-and-drop handlers to `DragEventArgs.DataTransfer` + `TryGetFiles()`, replaced deprecated `TextBox.Watermark` with `PlaceholderText`, and pinned `Tmds.DBus.Protocol` to 0.92.0 (patched, CVE-2026-39959); build is clean with 0 warnings and 234 managed tests pass.

- `#27` complete release-candidate stabilization, diagnostics hardening, and manual verification.
  - Status: `Closed`
  - Type: `Feature`
  - Milestone: `1.0`
  - Note: completed the 1.0 stabilization pass with the remaining startup, import, cleanup, and diagnostics hardening fixes, and the release-candidate UI flow is now covered by confirmed manual testing.

- `#93` improve diagnostics for converter launch failures and managed cover-loading failures.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: converter launch failures now include executable, working directory, command line, and Win32 error details, while UI cover-loading failures emit actionable error logs with the cover path and library root; native and managed regressions cover both diagnostics paths.

- `#92` decide and implement the real role of the `formats` table instead of keeping it as unused dual-write state.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: removed the unused `formats` dual-write path before 1.0 by making `books` the only canonical file-metadata store, adding a schema `v4` migration that drops obsolete `formats`, and covering both the migration and repository behavior with native regressions.

- `#91` make startup temp cleanup apply the same root-containment and reparse-point safety rules as managed file operations.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: startup `Temp` cleanup now uses shared managed-path deletion guards that remove in-root reparse points without traversing outside the library, with native regressions for both helper semantics and bootstrap junction cleanup.

- `#90` reduce browse and statistics query degradation on large libraries before the 1.0 release cut.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: read-side author sorting now uses a single aggregated join instead of a correlated subquery, and library statistics reuse cached cover-size totals until the database or observed cover snapshot changes, with native regression coverage for cache invalidation.

- `#89` harden managed storage commit rollback so partial file moves are recoverable and diagnosable after restore failures.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: managed-storage rollback now cleans up partially committed final paths and propagates restore-failure diagnostics through import failure results, with native regressions for both cleanup and error surfacing.

- `#88` add explicit disposal and lifetime cancellation to `LibraryBrowserViewModel` background refresh work.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: the library browser now cancels debounced and in-flight background work on disposal, and shell disposal explicitly tears down the browser slice with regression coverage for both lifetimes.

- `#87` clean up extracted ZIP import payloads after each import job instead of keeping them for the whole host session.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: ZIP entry payloads are now deleted immediately after each per-entry import attempt, with regressions proving `work/extracted` does not retain extracted files after ZIP jobs finish.

- `#77` fix the remaining April 8 review findings across rollback, delete status messaging, converter-probe lifecycle, and cover helper safety.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: import cancellation rollback now survives repository lookup failures, delete status text no longer falsely claims library Trash moves, pending converter probes are canceled on shell disposal, and cover placeholder palette selection is safe for all hash values.

- `#75` audit silent cleanup and rollback catch-all handlers and replace the actionable ones with structured diagnostics.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: actionable cleanup failures in import rollback, managed-storage restore, and trash diagnostics now emit structured warnings, with native regression coverage for rollback cleanup logging.

- `#74` fix `ImportJobsViewModel` success-notification flow so async subscribers are invoked deterministically and covered by tests.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: import success notifications now use an explicit result-carrying async handler contract, await every subscriber sequentially, and include multi-subscriber regression coverage.

- `#73` reduce orchestration sprawl in the shell and library browser view-models by extracting non-UI responsibilities into focused helpers without changing behavior.
  - Status: `Closed`
  - Type: `Feature`
  - Milestone: `1.0`
  - Note: extracted converter-probe lifecycle and cover/path-resolution into dedicated UI collaborators, keeping shell and library browser behavior unchanged while adding startup regression coverage for saved converter settings.

- `#72` replace raw Win32 handle management in `CoreHostProcess` with `SafeHandle`-backed lifetime management and failure-path verification.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: `CoreHostProcess` now owns shutdown-event and job-object handles through `SafeHandle`, releases both handles even when no process was assigned, and adds a regression test for the no-process cleanup path.

- `#71` reject cover-image paths that escape the active library root through symlinks or junctions before the UI attempts to load them.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: cover loading now canonicalizes existing paths through reparse-point resolution before root comparison, and managed UI regression coverage rejects a junction-based escape from the active library root.

- `#69` block drag-and-drop import while another import job is already running.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: window-level drag-and-drop now rejects new sources during active import, the drop zone dims with the rest of the import lock-out state, and a regression test keeps the running source text stable until the current job finishes.

- `#19` rework the visual style around a modern dark media-app direction, covering palette, typography, and a unified icon set.
  - Status: `Closed`
  - Type: `Feature`
  - Note: completed in `apps/Librova.UI` with a unified dark shell, shared style tokens, refreshed iconography, and accepted manual UI verification across `Library`, `Import`, `Settings`, startup/recovery states, and the application icon.

- `#2` restore live search filtering and full-list recovery after clearing the query.
  - Status: `Closed`
  - Type: `Bug`
  - Note: search now filters incrementally and restores the full list when the query is cleared.

- `#5` export books with a sanitized filename derived from title and author instead of the placeholder `book`.
  - Status: `Closed`
  - Type: `Feature`
  - Note: export now derives the output filename from title and author with path-safe sanitization.

- `#18` restore the duplicate import opt-in and treat accepted duplicates as fully independent managed books.
  - Status: `Closed`
  - Type: `Feature`
  - Note: duplicate import is now opt-in; accepted duplicates are stored as independent managed entries.

- `#20` add a stable hover state for book cards without layout jitter or text flicker.
  - Status: `Closed`
  - Type: `Bug`
  - Note: card hover is now stable with no layout shift or text flicker.

- `#24` add per-file native host logging for skipped and failed entries during multi-file, directory, and ZIP imports so large import diagnostics do not depend only on UI summary warnings.
  - Status: `Closed`
  - Type: `Feature`
  - Note: native host now logs each skipped or failed file individually during batch and ZIP imports.

- `#1` unblock `Export` and `Move to Trash` after the first successful directory import.
  - Status: `Closed`
  - Type: `Bug`
  - Note: real-host UI integration coverage now exercises both actions immediately after directory import, closing the stale `Needs Reproduction` gap and keeping the reported manual scenario under regression protection.

- `#12` show current library book count and managed-book size total in megabytes, excluding the database.
  - Status: `Closed`
  - Type: `Feature`
  - Note: library sidebar now shows book count and managed-files size in MB, excluding the database file.

- `#10` add explicit search mode selection: title or author, defaulting to title.
  - Status: `Closed`
  - Type: `Feature`
  - Note: superseded by the accepted global full-text search model that matches title, authors, tags, and description through one field.

- `#11` define and enforce minimum window dimensions that preserve multiple visible cards and at least two columns with the details panel open.
  - Status: `Closed`
  - Type: `Feature`
  - Note: minimum window size is now enforced so the grid always shows at least two columns with the details panel open.

- `#23` keep cover aspect ratios intact and use a neutral card background instead of exposed color bands.
  - Status: `Closed`
  - Type: `Bug`
  - Note: covers now preserve aspect ratio with letterbox fill; color-band artifacts removed.

- `#6` keep the selected card visible when the details panel opens or closes by centering it in the visible viewport.
  - Status: `Closed`
  - Type: `Bug`
  - Note: the selected card is now scrolled into view when the details panel opens or closes.

- `#29` replace fixed first-page library browsing with infinite scroll and total-result counts.
  - Status: `Closed`
  - Type: `Feature`
  - Note: the `Library` grid now appends additional pages on scroll, the counter shows the total matching result count instead of the currently loaded page size, language-filter options come from the full matching result set instead of only the already loaded cards, and delete refresh reloads the visible range in one read-side request instead of replaying paged RPC calls.

- `#25` add Windows `Recycle Bin` integration as a first-class delete flow instead of limiting delete to the managed-library `Trash` area.
  - Status: `Closed`
  - Type: `Feature`
  - Note: delete now stages through managed `Trash` for rollback safety, hands staged files off to the Windows `Recycle Bin`, reports managed-`Trash` fallback explicitly if that handoff fails, and cleans up empty `Trash` subfolders after successful handoff.

- `#34` extend metadata normalization beyond Russian-only case folding so search and duplicate detection stay correct for broader Cyrillic text.
  - Status: `Closed`
  - Type: `Bug`
  - Note: the shared metadata normalizer now lowercases extended Cyrillic code points including `І/і`, `Ї/ї`, `Є/є`, and `Ў/ў`, and regression coverage now proves both SQLite FTS search-index matching and probable-duplicate keys stay case-insensitive for those inputs.

- `#35` replace full-library probable-duplicate scans with indexed or query-level duplicate lookup.
  - Status: `Closed`
  - Type: `Feature`
  - Note: `SqliteBookQueryRepository::FindDuplicates` now keeps duplicate detection on one SQLite connection, narrows probable-duplicate candidates through an exact normalized-author-set query instead of reading the full library into memory, and preserves strict/probable semantics under regression tests including full author-set matching and multiple-match cases.

- `#36` separate language-list SQL binding from count-query binding in `SqliteBookQueryRepository`.
  - Status: `Closed`
  - Type: `Bug`
  - Note: `ListAvailableLanguages` now uses its own binder that matches the language-list SQL shape instead of reusing count-query binding with a phantom `language` parameter, and regression coverage now keeps combined `Series` / `Tags` / `Format` filters correct even when the current language filter is populated.

- `#37` remove fixed-sleep readiness from named-pipe tests and replace it with deterministic synchronization.
  - Status: `Closed`
  - Type: `Bug`
  - Note: named-pipe channel, host, and client tests now wait on explicit server-ready signaling before connecting instead of relying on `sleep_for(20ms)`, keeping IPC verification deterministic and aligned with the repository rule against fixed waits.

- `#38` make converter argument template expansion single-pass so literal placeholder text inside file paths is preserved.
  - Status: `Closed`
  - Type: `Bug`
  - Note: converter argument expansion now resolves placeholders in one explicit pass instead of repeatedly re-running replacement over already expanded text, and regression coverage now preserves literal `{output_format}`-style fragments when they appear inside source or destination paths.

- `#40` parse `series` and `genres` from `FB2` / `EPUB` and show them in book details.
  - Status: `Closed`
  - Type: `Feature`
  - Note: `FB2` parsing now extracts `<genre>` plus `sequence`, `EPUB` parsing now extracts `dc:subject` plus collection-based series metadata, and real-host/UI regressions keep those fields visible in the `Library` details panel while browse-time filtering remains tracked under `#26`.

- `#58` expose sort controls in the Library section and wire them to the existing backend sort implementation.
  - Status: `Closed`
  - Type: `Feature`
  - Milestone: `1.0`
  - Note: added `SortKeyOption` record and `AvailableSortKeys` to `LibraryCatalogModels`; wired `SelectedSortKey` state and `initialSortKey` constructor param into `LibraryBrowserViewModel`; updated `BuildRequest`/`BuildInitialRangeRequest` to pass `SortBy`; added sort `ComboBox` (`AppComboBox` class) to the Library toolbar with scroll-reset on change; persists and restores sort preference across sessions via `UiPreferencesSnapshot.PreferredSortKey`; preservation across library-root switches handled in `UiPreferencesSnapshotBuilder`.

- `#66` validate a configured converter by running a test conversion when the user saves converter settings.
  - Status: `Closed`
  - Type: `Feature`
  - Milestone: `1.0`
  - Note: added `Fb2ConverterProbe` (static C# class with 500 ms debounce) that writes a minimal embedded FB2 to a temp directory, runs `fb2cng convert --to epub2 --overwrite` with a 5 s timeout, and checks exit code plus output file; wired into `ShellViewModel` as an injectable delegate (`_converterProbe`); `IsConverterProbeInProgress` disables Save while the probe runs; on failure the validation message is shown in the existing error TextBlock and Save stays disabled; empty path clears the error immediately without running the probe; added `ConverterProbeStatusText` and a neutral status TextBlock in `SettingsView.axaml`; 6 new managed tests cover all probe outcome paths.

### Minor

- `#42` make compressed managed `FB2` storage visibly distinct from plain `.fb2` files inside the library.
  - Status: `Closed`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: compressed managed FB2 files now use the `.fb2.gz` extension instead of `.fb2`; implemented via an encoding-aware overload of `GetManagedFileName` and `GetManagedBookPath`; export and decompression are unaffected since they use the `storage_encoding` database column, not the file extension.

- `#67` add a Remove Converter button to Settings so the user can clear the configured converter in one action.
  - Status: `Closed`
  - Type: `Feature`
  - Milestone: `1.1`
  - Note: replaced the manual Save button with auto-save on successful probe completion or path clear; added an inline × (IconActionSm) button that clears the converter path; settings now apply automatically after validation without requiring an explicit save action.

- `#95` add the missing safety and lifecycle regressions for transport drift, rollback residue, portable containment, Unicode sort, and startup cleanup.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: the 1.0 safety gaps are now covered by regressions for managed mapper contract drift, cancellation rollback residue, portable relative-path containment, Cyrillic sorting, startup `Temp` junction cleanup, and bounded converter shutdown behavior.

- `#94` add missing regression coverage for damaged-library delete, duplicate-only batch import, forced conversion failure, and silent settings-state loss.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: the listed 1.0 blind spots are now covered by regressions for damaged-library delete with missing managed artifacts, duplicate-only batch and ZIP imports, forced EPUB conversion failures, and `SavePreferencesAsync` preserving `Fb2CngConfigPath`.

- `#57` disable the import options panel (Allow duplicate import, Force conversion to EPUB) during active import.
  - Status: `Closed`
  - Type: `Feature`
  - Note: added `IsEnabled="{Binding ImportJobs.IsBusy, Converter={x:Static BoolConverters.Not}}"` on the OPTIONS `Border.AppPanelMuted` in `ImportView.axaml`; the existing `Border.AppPanelMuted:disabled { Opacity: 0.45 }` style (added in #56) handles visual dimming; both checkboxes are disabled via parent `IsEnabled` propagation.

- `#56` dim the Library card in the sidebar during active import to match the disabled state of other sidebar elements.
  - Status: `Closed`
  - Type: `Feature`
  - Note: added `IsEnabled="{Binding Shell.IsImportInProgress, Converter={x:Static BoolConverters.Not}}"` on the Library card `Border` in `MainWindow.axaml`; added `Border.AppPanelMuted:disabled { Opacity: 0.45 }` to the GLOBAL DISABLED OPACITY section of `Components.axaml` — consistent with `Button.NavItem:disabled` opacity.

- `#55` open the main window on the Import tab after first-run library creation.
  - Status: `Closed`
  - Type: `Feature`
  - Note: `ShellViewModel` constructor now checks `session.HostOptions.LibraryOpenMode`; when `CreateNew`, `_currentSection` is set to `ShellSection.Import` instead of the default `Library`; regression test `Create_WithCreateNewOpenMode_DefaultsToImportSection` added to `ShellApplicationTests`.

- `#32` expand the left `Current Library` summary so library size includes managed books, covers, and the SQLite database instead of only managed-book files.
  - Status: `Closed`
  - Type: `Feature`
  - Note: the shell keeps one aggregate `Library: ...` size metric, and the statistics flow now aligns on total library size rather than a managed-books-only total.

- `#28` downscale oversized imported covers before storing them in the managed library.
  - Status: `Closed`
  - Type: `Feature`
  - Note: run cover optimization during import behind a native image-processing interface, keep aspect ratio, avoid upscaling, and keep the backend swappable so the initial Windows-only implementation does not leak into import policy.

- `#39` show the application version in the UI and define it in exactly one source of truth.
  - Status: `Closed`
  - Type: `Feature`
  - Note: `Settings` now shows the current application version, and both native `Core::Version` plus managed assembly metadata read it from the shared repository-root `VERSION.txt` file instead of duplicated per-project constants.

- `#54` replace the version-only line in Settings with a styled About card showing version, author name, and contact email.
  - Status: `Closed`
  - Type: `Feature`
  - Note: `Settings` now shows an `AppPanelCompact` About card with the Librova book icon, version, author name, and contact email; `ApplicationVersion` gained `Author` and `ContactEmail` static constants; `ShellViewModel` exposes `ApplicationAuthorText` and `ApplicationContactEmailText`; regression assertions added to the existing `ShellApplication` test.

- `#52` validate IPC-provided cover paths against the active library root on the UI side before file IO.
  - Status: `Closed`
  - Type: `Bug`
  - Note: `LibraryBrowserViewModel.LoadCoverImage()` now resolves the final path and rejects it via `IsWithinLibraryRoot()` before any file IO; absolute out-of-root paths and relative path-traversal attempts both return `null` with a warning log; two regression tests added.

- `#50` replace whole-file bootstrap log merging with streamed copy during UI log reinitialization.
  - Status: `Closed`
  - Type: `Bug`
  - Note: `ReinitializeUnsafe` now opens the previous log as a read `FileStream` and copies it into the destination via `Stream.CopyTo` instead of `File.ReadAllText` + `File.AppendAllText`; regression test verifies 5000-line bootstrap log is fully merged without truncation.

- `#51` save UI preferences atomically so interrupted writes do not corrupt the preferences file.
  - Status: `Closed`
  - Type: `Bug`
  - Note: `UiPreferencesStore.Save` now writes JSON to a sibling `.tmp` file then calls `File.Move(..., overwrite: true)` for an atomic replace; added regression tests for double-save overwrite correctness and `TryLoad` resilience against corrupted JSON.

- `#41` remove custom EPUB converter support and keep only built-in `fbc` / `fb2cng` configuration.
  - Status: `Closed`
  - Type: `Feature`
  - Note: the custom converter mode, arguments, and host flags are removed across settings, preferences, launch options, native converter configuration, and related tests; `Settings` now exposes only the built-in `fbc.exe` path while empty path cleanly disables EPUB conversion.

- `#3` display file sizes in megabytes instead of raw bytes.
  - Status: `Closed`
  - Type: `Feature`
  - Note: all file size displays now show values in MB rounded to one decimal place.

- `#4` remove the redundant top hero cards from tab pages.
  - Status: `Closed`
  - Type: `Feature`
  - Note: hero cards removed from Library, Import, and Settings tab headers.

- `#8` increase spacing between `Select Files` and `Select Folder`.
  - Status: `Closed`
  - Type: `Feature`
  - Note: gap between the two import action buttons increased to match the design grid.

- `#9` unify primary action button styling, starting with `Select Files` and `Select Folder`.
  - Status: `Closed`
  - Type: `Feature`
  - Note: both import buttons now use the unified `Button.AppPrimary` style.

- `#13` add a file-picker button for choosing the converter executable path.
  - Status: `Closed`
  - Type: `Feature`
  - Note: Settings now shows a Browse button that opens a file-picker for the `fbc.exe` path.

- `#14` remove the optional YAML config field for `fbc.exe` from the Settings UI.
  - Status: `Closed`
  - Type: `Feature`
  - Note: YAML-based converter path configuration removed; path is now managed only through the UI preference.

- `#16` move `Settings` to the bottom of the left navigation rail.
  - Status: `Closed`
  - Type: `Feature`
  - Note: Settings nav item repositioned to the bottom of the rail, separated from Library and Import.

- `#17` replace the technical subtitle with a product-facing personal-library slogan.
  - Status: `Closed`
  - Type: `Feature`
  - Note: startup subtitle updated to a user-facing personal library slogan.

- `#22` add an application icon for the window title bar and Windows taskbar.
  - Status: `Closed`
  - Type: `Feature`
  - Note: `.ico` asset added and registered; window title bar and taskbar now show the Librova book icon.

### Low

- `#98` remove the collision-path TOCTOU window in managed trash destination selection.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: managed trash destination selection now retries collisions around the actual move operation instead of pre-checking candidate paths, with a regression that simulates a competing file appearing during rename.

- `#97` replace named-pipe readable-byte busy polling with a more explicit wait strategy and cancellation-aware timeout handling.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: client-side timed named-pipe reads now use overlapped `ReadFile` with explicit event waits and `CancelIoEx` on timeout instead of `PeekNamedPipe` busy polling, with a native regression proving late responses do not keep timed-out calls hanging.

- `#99` stop recreating default UI state and preferences stores on every file-path property read in `ShellViewModel`.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: `ShellViewModel` now captures UI state and preferences file paths once during construction instead of rebuilding default stores on every diagnostics-property read, with a managed regression that proves the paths stay stable after creation.

- `#96` validate the second-stage WinAPI string conversion calls in `UnicodeConversion` instead of assuming they cannot fail.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: `WideToUtf8` and `Utf8ToWide` now validate both WinAPI conversion calls instead of trusting the write stage after a successful sizing pass, keeping the shared Unicode boundary fail-fast for storage, logging, and path handling.

- `#31` store managed `FB2` books in compressed form inside the library to reduce on-disk size, while preserving transparent browse, export, delete, duplicate-detection, and rollback behavior.
  - Status: `Closed`
  - Type: `Feature`
  - Note: completed as an internal managed-storage detail through persisted `storage_encoding`, with fallback-managed `FB2` compressed on import, transparently decoded on direct export and `Export As EPUB`, and library statistics now reflecting the real on-disk managed size.

- `#53` fail fast on unexpected `BOOK_FORMAT_ZIP` or unknown catalog format values instead of silently mapping them to `EPUB`.
  - Status: `Closed`
  - Type: `Bug`
  - Note: `FromProto(BookFormat)` in `LibraryCatalogProtoMapper` now explicitly rejects `BOOK_FORMAT_ZIP`, `BOOK_FORMAT_UNSPECIFIED`, and any unknown numeric value with an error log and `std::invalid_argument`; three regression tests cover the ZIP case via `BookListRequest`, via `ExportBookRequest`, and via an unknown numeric cast.

- `#43` show explicit busy or progress feedback during long-running `Export As EPUB` operations.
  - Status: `Closed`
  - Type: `Feature`
  - Note: added `IsExportBusy` property to `LibraryBrowserViewModel`; details panel now shows an indeterminate progress bar and status text while any export is in flight; EPUB export timeout increased to 2 min transport / 3 min outer to accommodate decompression and external converter time; regular export timeout set to 10 s / 30 s; regression test verifies `IsExportBusy` transitions during a gated export.

- `#45` prevent managed-trash cleanup from deleting required top-level library directories such as `Covers`.
  - Status: `Closed`
  - Type: `Bug`
  - Note: `CleanupEmptyParentDirectory` in `ManagedTrashService` now skips removal when the parent is a direct child of the library root; canonical root is computed once in the constructor and passed to the cleanup helper; two regression tests cover both the protection of `Covers/` and the allowed removal of per-book subdirectories.

- `#49` reject databases created by a newer schema version instead of silently downgrading `user_version`.
  - Status: `Closed`
  - Type: `Bug`
  - Note: `CSchemaMigrator::Migrate` now throws `std::runtime_error` with the detected and expected versions when `currentVersion > GetCurrentVersion()`; `user_version` is never overwritten in that path; regression test verifies both the throw and that the future version is preserved on disk.

- `#119` FB2 books with empty author nodes silently fail import instead of importing under "Аноним".
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: fixed — when all `<title-info><author>` name fields are empty (or the node is absent), the parser now emits a `[warning]` and stores `"Аноним"` as the author instead of throwing a hard error that skips the book entirely; test updated from "expect throw" to "expect Аноним in AuthorsUtf8".

- `#120` database file remains several MB after import cancellation instead of shrinking to near-zero.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: fixed — after a complete cancellation rollback (all book rows deleted), `CSqliteBookRepository::Compact()` (new override) executes `VACUUM;` to reclaim freed pages; `Compact()` is called from `CImportRollbackService::RollbackImportedBooks()` when `RemainingBookIds` is empty; the `IBookRepository` interface received a default no-op `Compact()` so test doubles require no changes.

- `#121` six FB2 genre codes from this import batch lack display-name mappings.
  - Status: `Closed`
  - Type: `Bug`
  - Milestone: `1.0`
  - Note: added `love_sf` (21 hits), `popular_business` (3), `marketing` (2), `architecture` (1), `sci_philology` (1), `sf_writing` (1) to `Fb2GenreMapper.cpp`; REQUIRE assertions added to `TestFb2GenreMapper.cpp`.
