# Librova Backlog

## 1. Working Model

Use a single backlog for active work.

Each task has:

- `Priority`: `Critical`, `Major`, `Minor`, `Low`
- `Status`: `Open`, `Needs Reproduction`, `Blocked`, `Closed`

Working rule:

- if the user asks to "move to the next task", take the highest-priority task with status `Open`;
- if a higher-priority task is marked `Needs Reproduction` or `Blocked`, take the next open task;
- if several tasks share the same priority, prefer the most local and easiest-to-verify task first.

## 2. Priority Meanings

- `Critical`: breaks a core user flow or leaves the current release scope functionally incomplete.
- `Major`: a strong UX or layout defect that noticeably hurts day-to-day use.
- `Minor`: local polish or visual consistency work without a broken flow.
- `Low`: useful but least urgent polish.

## 3. Open Backlog

### Critical

- `#30` add a reproducible Windows portable release packaging flow that produces a fully runnable distribution folder for `Librova.UI` plus `Librova.Core.Host`, with every required runtime dependency either statically linked or copied into the output.
  - Status: `Open`
  - Note: define the canonical packaging script, output layout, self-contained vs framework-dependent UI publish model, and a verification step on a clean Windows environment so the produced folder can be launched without repo-local build prerequisites.

- `#33` harden `CoreHostProcess` command-line quoting so core startup survives Windows paths with trailing backslashes and embedded quotes.
  - Status: `Closed`
  - Note: `CoreHostProcess` now escapes launch arguments with Windows-correct `CommandLineToArgvW` rules, and UI-side regression coverage verifies trailing `\`, embedded `"`, and Cyrillic paths through a real parse round-trip.

### Major

- `#26` strengthen `series` and `genres` support across parsing, storage, details, and browser filtering.
  - Status: `Open`
  - Note: treat `series` and `genres` as first-class metadata end-to-end instead of incidental parser output.

- `#27` complete release-candidate stabilization, diagnostics hardening, and manual verification.
  - Status: `Open`
  - Note: use this item for the remaining hardening pass instead of tracking stabilization in a separate standing-work section; startup now enforces explicit `Open Library` vs `Create Library` contracts, blocks silent in-place recreation for damaged libraries, keeps native CLI/logging Unicode-safe under Cyrillic library roots, keeps first-run bootstrap UI logs out of the chosen empty `Create Library` target until startup succeeds, uses explicit graceful host shutdown before any forced kill fallback, hardens free-text search against raw FTS punctuation input, and removes read-side `N+1` hydration from search plus probable-duplicate detection.

- `#34` extend metadata normalization beyond Russian-only case folding so search and duplicate detection stay correct for broader Cyrillic text.
  - Status: `Open`
  - Note: current normalization handles ASCII plus part of Russian Cyrillic, but misses characters such as `І/і`, `Ї/ї`, `Є/є`, and `Ў/ў`; fix the shared normalization path and add regression tests that prove search-indexing and duplicate keys remain case-insensitive for these inputs.

- `#35` replace full-library probable-duplicate scans with indexed or query-level duplicate lookup.
  - Status: `Open`
  - Note: `FindDuplicates` currently reads every stored title/author pair into memory on each import and opens multiple SQLite connections for one duplicate check; collapse that into a tighter database-backed path and keep strict/probable duplicate semantics unchanged under tests.

- `#36` separate language-list SQL binding from count-query binding in `SqliteBookQueryRepository`.
  - Status: `Open`
  - Note: `ListAvailableLanguages` currently reuses `BindSearchCountFilters` even though its SQL intentionally omits the `language` placeholder; the current UI path resets `Language` before calling it, but the repository method is still internally inconsistent and should get its own binder plus coverage for combined filters.

- `#37` remove fixed-sleep readiness from named-pipe tests and replace it with deterministic synchronization.
  - Status: `Open`
  - Note: several IPC tests still rely on `sleep_for(20ms)` before connecting clients; replace those sleeps with explicit readiness signaling so host/channel/client tests follow the repository rule against fixed waits.

### Minor

- `#32` expand the left `Current Library` summary so library size includes managed books, covers, and the SQLite database instead of only managed-book files.
  - Status: `Open`
  - Note: decide whether the UI should show a single total-library-size metric or an explicit breakdown for `Books`, `Covers`, and `Database`, then align product docs and statistics contracts with that choice.

- `#38` make converter argument template expansion single-pass so literal placeholder text inside file paths is preserved.
  - Status: `Open`
  - Note: current replacement logic can re-expand `{output_format}` and similar tokens if they appear in the source or destination path text; keep placeholder substitution explicit and add a regression test for filenames containing literal placeholder-like fragments.

- `#28` downscale oversized imported covers before storing them in the managed library.
  - Status: `Closed`
  - Note: run cover optimization during import behind a native image-processing interface, keep aspect ratio, avoid upscaling, and keep the backend swappable so the initial Windows-only implementation does not leak into import policy.

### Low

- `#31` store managed `FB2` books in compressed form inside the library to reduce on-disk size, while preserving transparent browse, export, delete, duplicate-detection, and rollback behavior.
  - Status: `Open`
  - Note: scope this explicitly to `FB2` first instead of a generic “all uncompressed formats”, and define whether compression is an internal storage detail or a surfaced managed-format variant across database, transport, statistics, and export flows.

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

### Critical

- `#15` wire configured `fbc.exe` / `fb2cng` usage into import and export flows, with `Export As EPUB` and forced-import-conversion UI affordances enabled only when the converter path is configured for the current session.
  - Status: `Closed`

- `#7` show deterministic import progress for large folders, including total file count, processed count, and final outcome summary.
  - Status: `Closed`

### Minor

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

## 5. Done Criteria

A task can be considered closed only when:

- the behavior is actually changed in code;
- appropriate automated tests are added where practical;
- documentation is updated if user-visible behavior or working rules changed;
- the user confirms manual verification when the task comes from UI or manual-test remarks.

## 6. History

Verified change history is maintained in:

- [Development-History](archive/Development-History.md)
