# Librova Backlog

## 1. Working Model

Use a single backlog for active work.

Each task has:

- `Priority`: `Critical`, `Major`, `Minor`, `Low`
- `Status`: `Open`, `Needs Reproduction`, `Blocked`, `Closed`

Working rule:

- if the user asks to "move to the next task", take the highest-priority task with status `Open`;
- if a higher-priority task is marked `Needs Reproduction` or `Blocked`, take the next open task;
- if several tasks share the same priority, prefer the most local and easiest-to-verify task first;
- when a task is completed, move it out of `Open Backlog` into the matching priority section under `Closed Backlog` in the same change;
- `Open Backlog` must never contain an item whose status is `Closed`.

Backlog edit rules:

- every task entry must be exactly three lines in this shape:
  `- #<id> <summary>`
  `  - Status: <status>`
  `  - Note: <note>`
- every new task must get a new unique id that is greater than every existing task id in the document;
- every new open task must be inserted only under `## 3. Open Backlog` in the matching priority section;
- when taking a task into work, do not duplicate, rename, or move it yet unless reprioritization is intentional; keep it in `Open Backlog` with status `Open`, `Needs Reproduction`, or `Blocked`;
- when closing a task, in the same change remove the full three-line entry from `Open Backlog` and add the full three-line entry under the matching priority section in `## 4. Closed Backlog` with status `Closed`;
- after any backlog edit, verify there are no orphan task-title lines without `Status` and `Note`, and that no task id appears in both `Open Backlog` and `Closed Backlog`.

## 2. Priority Meanings

- `Critical`: breaks a core user flow or leaves the current release scope functionally incomplete.
- `Major`: a strong UX or layout defect that noticeably hurts day-to-day use.
- `Minor`: local polish or visual consistency work without a broken flow.
- `Low`: useful but least urgent polish.

## 3. Open Backlog

### Critical
- `#44` store managed book and cover paths as stable library-relative values instead of absolute filesystem paths.
  - Status: `Open`
  - Note: current import persistence writes absolute managed paths into SQLite, which breaks library portability and drifts from the transport/tests that expect `Books/...` and `Covers/...` relative paths; fix persistence, read-side resolution, and any affected transport mapping together.

- `#45` prevent managed-trash cleanup from deleting required top-level library directories such as `Covers`.
  - Status: `Open`
  - Note: deleting the last cover currently allows post-move cleanup to remove `LibraryRoot/Covers`, which then causes existing-library bootstrap validation to fail on the next startup; keep cleanup below the protected top-level layout only.

- `#46` make staged delete crash-safe across filesystem moves and catalog removal.
  - Status: `Open`
  - Note: the current delete flow moves managed files into library `Trash` before removing the database row, so a crash in between can leave a catalog entry pointing at a missing managed file; tighten ordering or add durable recovery semantics so delete remains consistent across process interruption.

- `#47` harden duplicate protection against concurrent imports with database-level enforcement.
  - Status: `Open`
  - Note: duplicate detection currently relies on read-side `FindDuplicates` checks plus a non-unique `sha256_hex` index, which still allows parallel import races to insert the same book twice; add a schema-level uniqueness barrier and make the write path handle the conflict explicitly.

- `#48` compute and propagate SHA-256 for batch and ZIP imports so strict duplicate detection works beyond single-source manual requests.
  - Status: `Open`
  - Note: the normal UI and multi-source import flow do not populate `sha256_hex`, so strict duplicate detection by content hash is effectively absent for batch and ZIP imports; compute the hash in the real import pipeline and cover it with regression tests.

### Major
- `#49` reject databases created by a newer schema version instead of silently downgrading `user_version`.
  - Status: `Open`
  - Note: schema migration currently accepts forward-version databases and then unconditionally rewrites `PRAGMA user_version` to the local version, which risks running an older binary against an incompatible schema; fail fast with explicit recovery guidance.

- `#26` complete first-class browser support for `series` and `genres` once metadata parsing and details display are in place.
  - Status: `Open`
  - Note: focus this item on browse-time behavior: filter sources, filter UI, request plumbing, result counts, and any related series/genres browsing flows rather than parser-only metadata extraction.

- `#27` complete release-candidate stabilization, diagnostics hardening, and manual verification.
  - Status: `Open`
  - Note: use this item for the remaining hardening pass instead of tracking stabilization in a separate standing-work section; startup now enforces explicit `Open Library` vs `Create Library` contracts, blocks silent in-place recreation for damaged libraries, keeps native CLI/logging Unicode-safe under Cyrillic library roots, keeps first-run bootstrap UI logs out of the chosen empty `Create Library` target until startup succeeds, uses explicit graceful host shutdown before any forced kill fallback, hardens free-text search against raw FTS punctuation input, and removes read-side `N+1` hydration from search plus probable-duplicate detection.
### Minor
- `#50` replace whole-file bootstrap log merging with streamed copy during UI log reinitialization.
  - Status: `Closed`
  - Note: `ReinitializeUnsafe` now opens the previous log as a read `FileStream` and copies it into the destination via `Stream.CopyTo` instead of `File.ReadAllText` + `File.AppendAllText`; regression test verifies 5000-line bootstrap log is fully merged without truncation.

- `#51` save UI preferences atomically so interrupted writes do not corrupt the preferences file.
  - Status: `Closed`
  - Note: `UiPreferencesStore.Save` now writes JSON to a sibling `.tmp` file then calls `File.Move(..., overwrite: true)` for an atomic replace; added regression tests for double-save overwrite correctness and `TryLoad` resilience against corrupted JSON.

- `#54` replace the version-only line in Settings with a styled About card showing version, author name, and contact email.
  - Status: `Closed`
  - Note: `Settings` now shows an `AppPanelCompact` About card with the Librova book icon, version, author name, and contact email; `ApplicationVersion` gained `Author` and `ContactEmail` static constants; `ShellViewModel` exposes `ApplicationAuthorText` and `ApplicationContactEmailText`; regression assertions added to the existing `ShellApplication` test.

- `#42` make compressed managed `FB2` storage visibly distinct from plain `.fb2` files inside the library.
  - Status: `Open`
  - Note: fallback-managed compressed `FB2` files currently keep the original `.fb2` extension even though their on-disk bytes are no longer plain `FB2`, which makes manual inspection of the managed library misleading; define and implement a clearer internal naming/layout rule for that storage representation.

- `#55` open the main window on the Import tab after first-run library creation.
  - Status: `Open`
  - Note: when the user creates a new library on first run, the shell currently opens on the Library tab; since the library is empty at that point the Library tab shows nothing useful; switch the initial active tab to Import so the user is immediately in context to add books.

### Low
- `#43` show explicit busy or progress feedback during long-running `Export As EPUB` operations.
  - Status: `Open`
  - Note: when `Export As EPUB` needs to decode a compressed managed `FB2` and then run conversion, the UI currently looks idle while no output file has appeared yet; add visible export-in-progress feedback such as disabled actions, wait cursor, status text, or progress so the running operation is obvious.

## 4. Closed Backlog

### Major

- `#19` rework the visual style around a modern dark media-app direction, covering palette, typography, and a unified icon set.
  - Status: `Closed`
  - Note: completed in `apps/Librova.UI` with a unified dark shell, shared style tokens, refreshed iconography, and accepted manual UI verification across `Library`, `Import`, `Settings`, startup/recovery states, and the application icon.

- `#2` restore live search filtering and full-list recovery after clearing the query.
  - Status: `Closed`

- `#5` export books with a sanitized filename derived from title and author instead of the placeholder `book`.
  - Status: `Closed`

- `#18` restore the duplicate import opt-in and treat accepted duplicates as fully independent managed books.
  - Status: `Closed`

- `#20` add a stable hover state for book cards without layout jitter or text flicker.
  - Status: `Closed`

- `#24` add per-file native host logging for skipped and failed entries during multi-file, directory, and ZIP imports so large import diagnostics do not depend only on UI summary warnings.
  - Status: `Closed`

- `#1` unblock `Export` and `Move to Trash` after the first successful directory import.
  - Status: `Closed`
  - Note: real-host UI integration coverage now exercises both actions immediately after directory import, closing the stale `Needs Reproduction` gap and keeping the reported manual scenario under regression protection.

- `#12` show current library book count and managed-book size total in megabytes, excluding the database.
  - Status: `Closed`

- `#10` add explicit search mode selection: title or author, defaulting to title.
  - Status: `Closed`
  - Note: superseded by the accepted global full-text search model that matches title, authors, tags, and description through one field.

- `#11` define and enforce minimum window dimensions that preserve multiple visible cards and at least two columns with the details panel open.
  - Status: `Closed`

- `#23` keep cover aspect ratios intact and use a neutral card background instead of exposed color bands.
  - Status: `Closed`

- `#6` keep the selected card visible when the details panel opens or closes by centering it in the visible viewport.
  - Status: `Closed`

- `#29` replace fixed first-page library browsing with infinite scroll and total-result counts.
  - Status: `Closed`
  - Note: the `Library` grid now appends additional pages on scroll, the counter shows the total matching result count instead of the currently loaded page size, language-filter options come from the full matching result set instead of only the already loaded cards, and delete refresh reloads the visible range in one read-side request instead of replaying paged RPC calls.

- `#25` add Windows `Recycle Bin` integration as a first-class delete flow instead of limiting delete to the managed-library `Trash` area.
  - Status: `Closed`
  - Note: delete now stages through managed `Trash` for rollback safety, hands staged files off to the Windows `Recycle Bin`, reports managed-`Trash` fallback explicitly if that handoff fails, and cleans up empty `Trash` subfolders after successful handoff.

- `#34` extend metadata normalization beyond Russian-only case folding so search and duplicate detection stay correct for broader Cyrillic text.
  - Status: `Closed`
  - Note: the shared metadata normalizer now lowercases extended Cyrillic code points including `І/і`, `Ї/ї`, `Є/є`, and `Ў/ў`, and regression coverage now proves both SQLite FTS search-index matching and probable-duplicate keys stay case-insensitive for those inputs.

- `#35` replace full-library probable-duplicate scans with indexed or query-level duplicate lookup.
  - Status: `Closed`
  - Note: `SqliteBookQueryRepository::FindDuplicates` now keeps duplicate detection on one SQLite connection, narrows probable-duplicate candidates through an exact normalized-author-set query instead of reading the full library into memory, and preserves strict/probable semantics under regression tests including full author-set matching and multiple-match cases.

- `#36` separate language-list SQL binding from count-query binding in `SqliteBookQueryRepository`.
  - Status: `Closed`
  - Note: `ListAvailableLanguages` now uses its own binder that matches the language-list SQL shape instead of reusing count-query binding with a phantom `language` parameter, and regression coverage now keeps combined `Series` / `Tags` / `Format` filters correct even when the current language filter is populated.

- `#37` remove fixed-sleep readiness from named-pipe tests and replace it with deterministic synchronization.
  - Status: `Closed`
  - Note: named-pipe channel, host, and client tests now wait on explicit server-ready signaling before connecting instead of relying on `sleep_for(20ms)`, keeping IPC verification deterministic and aligned with the repository rule against fixed waits.

- `#38` make converter argument template expansion single-pass so literal placeholder text inside file paths is preserved.
  - Status: `Closed`
  - Note: converter argument expansion now resolves placeholders in one explicit pass instead of repeatedly re-running replacement over already expanded text, and regression coverage now preserves literal `{output_format}`-style fragments when they appear inside source or destination paths.

- `#40` parse `series` and `genres` from `FB2` / `EPUB` and show them in book details.
  - Status: `Closed`
  - Note: `FB2` parsing now extracts `<genre>` plus `sequence`, `EPUB` parsing now extracts `dc:subject` plus collection-based series metadata, and real-host/UI regressions keep those fields visible in the `Library` details panel while browse-time filtering remains tracked under `#26`.

### Critical

- `#30` add a reproducible Windows portable release packaging flow that produces a fully runnable distribution folder for `Librova.UI` plus `Librova.Core.Host`, with every required runtime dependency either statically linked or copied into the output.
  - Status: `Closed`
  - Note: `scripts/PublishPortable.ps1` is now the canonical Windows packaging flow. It publishes `Librova.UI` as a self-contained single-file `win-x64` executable, builds `Librova.Core.Host` through the `x64-release-static` preset so the host can ship as a single native `exe`, copies any remaining runtime dependencies into `out/package/win-x64-portable/`, and runs a portable smoke test before reporting success. The portable package now also keeps bootstrap UI state, preferences, and pre-library logs under `PortableData/` beside the executables instead of `%LOCALAPPDATA%`, while active library logs still move into `LibraryRoot/Logs`. The verified output currently contains `Librova.UI.exe`, `LibrovaCoreHostApp.exe`, and the Avalonia native runtime files `av_libglesv2.dll`, `libHarfBuzzSharp.dll`, and `libSkiaSharp.dll`.

- `#15` wire configured `fbc.exe` / `fb2cng` usage into import and export flows, with `Export As EPUB` and forced-import-conversion UI affordances enabled only when the converter path is configured for the current session.
  - Status: `Closed`

- `#7` show deterministic import progress for large folders, including total file count, processed count, and final outcome summary.
  - Status: `Closed`

- `#33` harden `CoreHostProcess` command-line quoting so core startup survives Windows paths with trailing backslashes and embedded quotes.
  - Status: `Closed`
  - Note: `CoreHostProcess` now escapes launch arguments with Windows-correct `CommandLineToArgvW` rules, and UI-side regression coverage verifies trailing `\`, embedded `"`, and Cyrillic paths through a real parse round-trip.

### Minor

- `#32` expand the left `Current Library` summary so library size includes managed books, covers, and the SQLite database instead of only managed-book files.
  - Status: `Closed`
  - Note: the shell keeps one aggregate `Library: ...` size metric, and the statistics flow now aligns on total library size rather than a managed-books-only total.

- `#28` downscale oversized imported covers before storing them in the managed library.
  - Status: `Closed`
  - Note: run cover optimization during import behind a native image-processing interface, keep aspect ratio, avoid upscaling, and keep the backend swappable so the initial Windows-only implementation does not leak into import policy.

- `#39` show the application version in the UI and define it in exactly one source of truth.
  - Status: `Closed`
  - Note: `Settings` now shows the current application version, and both native `Core::Version` plus managed assembly metadata read it from the shared repository-root `VERSION.txt` file instead of duplicated per-project constants.

- `#54` replace the version-only line in Settings with a styled About card showing version, author name, and contact email.
  - Status: `Closed`
  - Note: `Settings` now shows an `AppPanelCompact` About card with the Librova book icon, version, author name, and contact email; `ApplicationVersion` gained `Author` and `ContactEmail` static constants; `ShellViewModel` exposes `ApplicationAuthorText` and `ApplicationContactEmailText`; regression assertions added to the existing `ShellApplication` test.

- `#52` validate IPC-provided cover paths against the active library root on the UI side before file IO.
  - Status: `Closed`
  - Note: `LibraryBrowserViewModel.LoadCoverImage()` now resolves the final path and rejects it via `IsWithinLibraryRoot()` before any file IO; absolute out-of-root paths and relative path-traversal attempts both return `null` with a warning log; two regression tests added.

- `#50` replace whole-file bootstrap log merging with streamed copy during UI log reinitialization.
  - Status: `Closed`
  - Note: `ReinitializeUnsafe` now opens the previous log as a read `FileStream` and copies it into the destination via `Stream.CopyTo` instead of `File.ReadAllText` + `File.AppendAllText`; regression test verifies 5000-line bootstrap log is fully merged without truncation.

- `#51` save UI preferences atomically so interrupted writes do not corrupt the preferences file.
  - Status: `Closed`
  - Note: `UiPreferencesStore.Save` now writes JSON to a sibling `.tmp` file then calls `File.Move(..., overwrite: true)` for an atomic replace; added regression tests for double-save overwrite correctness and `TryLoad` resilience against corrupted JSON.

- `#41` remove custom EPUB converter support and keep only built-in `fbc` / `fb2cng` configuration.
  - Status: `Closed`
  - Note: the custom converter mode, arguments, and host flags are removed across settings, preferences, launch options, native converter configuration, and related tests; `Settings` now exposes only the built-in `fbc.exe` path while empty path cleanly disables EPUB conversion.

- `#3` display file sizes in megabytes instead of raw bytes.
  - Status: `Closed`

- `#4` remove the redundant top hero cards from tab pages.
  - Status: `Closed`

- `#8` increase spacing between `Select Files` and `Select Folder`.
  - Status: `Closed`

- `#9` unify primary action button styling, starting with `Select Files` and `Select Folder`.
  - Status: `Closed`

- `#13` add a file-picker button for choosing the converter executable path.
  - Status: `Closed`

- `#14` remove the optional YAML config field for `fbc.exe` from the Settings UI.
  - Status: `Closed`

- `#16` move `Settings` to the bottom of the left navigation rail.
  - Status: `Closed`

- `#17` replace the technical subtitle with a product-facing personal-library slogan.
  - Status: `Closed`

- `#22` add an application icon for the window title bar and Windows taskbar.
  - Status: `Closed`

### Low

- `#31` store managed `FB2` books in compressed form inside the library to reduce on-disk size, while preserving transparent browse, export, delete, duplicate-detection, and rollback behavior.
  - Status: `Closed`
  - Note: completed as an internal managed-storage detail through persisted `storage_encoding`, with fallback-managed `FB2` compressed on import, transparently decoded on direct export and `Export As EPUB`, and library statistics now reflecting the real on-disk managed size.

- `#53` fail fast on unexpected `BOOK_FORMAT_ZIP` or unknown catalog format values instead of silently mapping them to `EPUB`.
  - Status: `Closed`
  - Note: `FromProto(BookFormat)` in `LibraryCatalogProtoMapper` now explicitly rejects `BOOK_FORMAT_ZIP`, `BOOK_FORMAT_UNSPECIFIED`, and any unknown numeric value with an error log and `std::invalid_argument`; three regression tests cover the ZIP case via `BookListRequest`, via `ExportBookRequest`, and via an unknown numeric cast.

## 5. Done Criteria

A task can be considered closed only when:

- the behavior is actually changed in code;
- appropriate automated tests are added where practical;
- documentation is updated if user-visible behavior or working rules changed;
- the user confirms manual verification when the task comes from UI or manual-test remarks.
