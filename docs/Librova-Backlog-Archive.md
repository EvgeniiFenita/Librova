# Librova Backlog — Archive

This file contains all closed backlog tasks.
It is not required reading during normal development — open tasks live in `Librova-Backlog.md`.
Consult this file when you need to look up past work, verify what was done, or confirm that a task id is not being reused.

## Closed Backlog

### Critical

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
