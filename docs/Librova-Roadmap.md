# Librova Roadmap

## 1. Current Stage

Librova has a working MVP baseline, but active MVP work is still continuing.

Already implemented end-to-end:

- single-file, multi-file, directory, and archive import;
- browser and details;
- export;
- first-run setup;
- settings for converter configuration;
- logging, host lifecycle, and manual validation baseline.

## 2. Remaining MVP Work

The remaining active MVP scope is:

- stronger support for series and genres across parsing, storage, details, and browser filtering;
- broad review passes on runtime behavior and crash safety;
- remediation of the manual-test backlog across import, search, export, settings, shell layout, and visual consistency;
- final cleanup of logging and error surfaces;
- manual UI verification against the maintained scenario checklist;
- final release-candidate hardening;
- documentation cleanup when implemented reality changes.

Library browsing polish that is explicitly deferred until a later library UX iteration:

- broader library UX polish beyond the MVP-safe series/genres work;
- storage sharding or per-book storage-layout redesign.

### 2.1 Manual Test Remediation Backlog

The latest manual test pass produced a concrete MVP remediation backlog. Work should be prioritized by user-facing risk first and by implementation complexity second.

#### P0 — Functional correctness and broken user flows

- `#1` unblock `Export` and `Move to Trash` after the first successful directory import;
- `#2` restore live search filtering and full-list recovery after clearing the query;
- `#5` export books with a sanitized filename derived from title and author instead of the placeholder `book`;
- `#7` show deterministic import progress for large folders, including total file count, processed count, and final outcome summary;
- `#15` wire configured `fbc.exe` / `fb2cng` usage into import and export flows, with UI affordances enabled only when the converter path is configured;
- `#18` restore the duplicate import opt-in and treat accepted duplicates as fully independent managed books.

#### P1 — High-value MVP usability and layout fixes

- `#6` keep the selected card visible when the details panel opens or closes by centering it in the visible viewport;
- `#10` add explicit search mode selection: title or author, defaulting to title;
- `#11` define and enforce minimum window dimensions that preserve multiple visible cards and at least two columns with the details panel open;
- `#12` show current library book count and managed-book size total in megabytes, excluding the database;
- `#13` add a file-picker button for choosing the converter executable path;
- `#14` remove the optional YAML config field for `fbc.exe` from the MVP UI;
- `#16` move `Settings` to the bottom of the left navigation rail;
- `#23` keep cover aspect ratios intact and use a neutral card background instead of exposed color bands.

#### P2 — Targeted polish with low or moderate implementation risk

- `#3` display file sizes in megabytes instead of raw bytes;
- `#4` remove the redundant top hero cards from tab pages;
- `#8` increase spacing between `Select Files` and `Select Folder`;
- `#9` unify primary action button styling, starting with `Select Files` and `Select Folder`;
- `#17` replace the technical subtitle with a product-facing personal-library slogan;
- `#20` add a stable hover state for book cards without layout jitter or text flicker;
- `#22` add an application icon for the window title bar and Windows taskbar.

#### P3 — Cohesive visual refresh required by the current MVP pass

- `#19` replace the current palette with a modern dark theme using one bright accent color across the shell;
- `#21` refresh typography using fonts that are practically available in Avalonia packaging and deployment for Windows.

#### Complexity Bands

Low complexity:

- `#3`, `#4`, `#8`, `#9`, `#13`, `#14`, `#16`, `#17`, `#22`

Medium complexity:

- `#1`, `#2`, `#5`, `#6`, `#10`, `#11`, `#12`, `#18`, `#20`, `#23`

High complexity:

- `#7`, `#15`, `#19`, `#21`

## 3. MVP Blockers From Review Pass

The release-blocking runtime issues confirmed by the review pass have now been closed and re-verified:

- SQLite database opening is Unicode-safe on Windows.
- Host shutdown no longer relies on watchdog-driven `ExitProcess`.
- External converter child processes are bound to a Windows `Job Object`, and cancellation cleans up produced output files.

## 4. Iterative Stabilization Plan

Work should move in small checkpoints, each ending with updated tests and docs when implemented reality changes.

### Iteration 1 — SQLite Unicode Path Safety

Status: completed and verified.

- switch native SQLite open path handling to explicit UTF-8 conversion;
- add verification that a database under a Unicode or Cyrillic path opens and migrates successfully.

### Iteration 2 — Graceful Host Shutdown

Status: completed and verified.

- replace watchdog-driven `ExitProcess` termination with a graceful shutdown signal;
- ensure destructor-based cleanup, SQLite close, and log flushing remain on the normal process-exit path;
- add verification for parent-process termination behavior.

### Iteration 3 — Converter Lifetime Safety

Status: completed and verified.

- bind external converter processes to a Windows `Job Object` with kill-on-close semantics;
- keep cancellation as a distinct outcome and clean up converter outputs after cancellation;
- add verification that cancelled or aborted sessions do not leave converter processes or partial outputs behind.

### Iteration 4 — SQLite Runtime Hardening

Status: completed and verified.

- add `busy_timeout` protection for concurrent read/write access patterns;
- verify search and import behavior under overlapping database access.

### Iteration 5 — FB2 Parsing Hardening

Status: completed and verified.

- replace locale-dependent numeric parsing in FB2 metadata extraction;
- keep Windows-1251 support while tightening parser behavior through targeted tests.

### Iteration 6 — IPC Contract Guardrails

Status: completed and verified.

- add explicit validation that C++ and C# pipe method enums stay synchronized;
- treat transport drift checks as part of routine validation for future RPC work.

### Iteration 7 — Logging And Failure Diagnostics

Status: completed and verified.

- stop silently swallowing rollback failures in trash-related recovery paths;
- make export overwrite behavior explicit in code, logs, and, if needed, the user-facing contract.

### Iteration 8 — Code Health Cleanup

- Status: completed and verified.
- remove duplicated path-safety and UTF-8 helper logic where it creates maintenance risk;
- harden ZIP archive wrappers against accidental copy semantics;
- clean up smaller review-pass issues that are real but not release-blocking.

### Iteration 9 — Import, Search, And Export Recovery

Status: planned.

- close the broken post-import action-state bug on `Export` and `Move to Trash`;
- restore live query filtering and correct reset behavior when the search box is cleared;
- replace placeholder export filenames with sanitized `Title + Author` naming;
- restore duplicate opt-in in the import UI and persistence flow, with explicit independent-record semantics;
- add regression coverage for the exact broken user flows found in manual testing.

### Iteration 10 — Converter And Import Feedback Completion

Status: planned.

- add a converter executable picker and remove the MVP-unneeded YAML config input;
- integrate configured `fbc.exe` / `fb2cng` into forced FB2-to-EPUB import and optional EPUB export conversion;
- keep converter-dependent controls disabled when no executable path is configured;
- redesign large-folder import progress around pre-counted totals, processed counts, and a final success / skipped summary;
- add tests for converter enablement, duplicate independence, and import-summary behavior.

### Iteration 11 — Library UX And Shell Layout Hardening

Status: planned.

- keep the selected book card centered when the details panel changes layout width;
- add search-mode selection for title vs. author;
- enforce minimum shell dimensions derived from card-grid and details-panel constraints;
- show current library counts and managed-book size totals in megabytes;
- move `Settings` to a bottom-anchored shell position;
- preserve cover aspect ratios and switch to a neutral background treatment for letterboxed covers.

### Iteration 12 — Visual Consolidation And Interaction Polish

Status: planned.

- remove redundant hero cards from tab pages to reclaim vertical space;
- normalize button colors and spacing across the shell, starting with import actions;
- update the product-facing subtitle copy;
- add a stable, non-jittering hover state for book cards;
- convert raw byte displays to megabytes where shown;
- add the application icon and align remaining small-shell presentation details.

### Iteration 13 — MVP Theme And Typography Refresh

Status: planned.

- replace the current palette with a cohesive dark visual system inspired by modern media applications while keeping interaction states legible;
- choose and integrate a modern font pairing that is realistic for Avalonia on Windows, including packaging implications;
- re-verify contrast, readability, and layout density after the palette and typography shift;
- update manual UI scenarios if visible workflows or expectations change.

## 5. Release Readiness Criteria

Librova can be treated as MVP-ready when:

- `Debug` and `Release` validation stay green;
- the confirmed MVP blockers from the review pass are closed and re-verified;
- manual UI test scenarios have been walked through successfully;
- the manual-test remediation backlog is closed at least through `P1`, and planned `P2` / `P3` items required for the current MVP presentation pass are verified;
- series/genres support is implemented and validated;
- no critical runtime regressions remain in startup, import, browser, export, delete, or settings flows;
- logs are actionable enough to diagnose startup and runtime problems.

## 6. Out Of Scope For The Current MVP

Not on the active MVP path:

- built-in reading experience;
- metadata editing;
- cloud or sync features;
- multiple libraries;
- plugin system;
- Windows `Recycle Bin` integration as a future delete-flow feature;
- storage sharding and other storage-layout refactors without a proven MVP need;
- large convenience features that do not reduce MVP risk.

## 7. History

Verified checkpoint history is preserved in:

- [Development-History](archive/Development-History.md)
