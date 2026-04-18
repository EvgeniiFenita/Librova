# Librova Release Checklist

Flat checklist for manual release verification. Items without `(RG)` are recommended checks; items marked `**(RG)**` are mandatory release gates.

---

## Startup & Navigation

- First-run wizard: Create New (empty folder) **(RG)**
- First-run wizard: Open Existing (valid library) **(RG)**
- First-run validation: relative path is rejected **(RG)**
- First-run validation: non-empty non-library folder is rejected for Create **(RG)**
- First-run validation: non-library folder is rejected for Open **(RG)**
- Cyrillic library path: startup and logs show no mojibake **(RG)**
- Portable package: the library reopens after drive/path relocation without config edits
- Startup Error Recovery: invalid `PreferredLibraryRoot` shows the recovery screen **(RG)**
- Startup Error Recovery: recovery with a new path successfully loads the library **(RG)**
- Corrupted library: the recovery screen does not try to recreate the library and accepts a different path **(RG)**
- Shell navigation: `Library` / `Import` / `Settings` switch correctly and content does not overlap
- Application icon is present in the window title bar
- Host lifetime: host exits together with the UI on normal close **(RG)**
- Host lifetime: host exits after force-killing the UI **(RG)**

---

## Import

- Source validation: `.txt` is rejected with an error **(RG)**
- Source validation: `.fb2`, `.epub`, `.zip` are accepted and import starts automatically **(RG)**
- Batch: mixed valid and invalid files — valid files import, invalid files appear in warnings **(RG)**
- Folder: recursive import **(RG)**
- Folder: empty folder produces a warning in the summary and does not block the rest of the import
- ZIP: parallel import of multiple files from the archive **(RG)**
- ZIP: corrupted archive — a valid book from the same archive still imports **(RG)**
- Progress summary in the `RUNNING` state shows total / processed / imported / failed / skipped
- Cancellation: rollback of already imported books **(RG)**
- Cancellation: the library returns to the pre-import state **(RG)**
- Drag-and-drop: files start import automatically **(RG)**
- Drag-and-drop: folder starts import automatically **(RG)**
- Drag-and-drop: multiple files at once
- After import completes, the Library browser refreshes automatically **(RG)**
- Duplicate override: `Allow duplicate import` creates a separate record in the library **(RG)**
- `Force conversion to EPUB during import` is visible only when a converter is configured

---

## Library Browser

- Book grid is visible and infinite scroll works **(RG)**
- Empty state is shown for an empty library
- Book details: metadata, annotation, and size in MB are visible **(RG)**
- Details panel does not overlap the grid
- With the details panel open: at least 2 card columns and at least 2 visible rows
- Search: live update without pressing Enter **(RG)**
- FTS search works across title, authors, tags, and description **(RG)**
- Search: punctuation in the query does not break FTS **(RG)**
- Filters popup: `LANGUAGES` and `GENRES` groups are visible **(RG)**
- Filters popup: OR semantics within a group, AND semantics between groups **(RG)**
- Filters button shows `Filters · N` and turns amber when filters are active **(RG)**
- Filters popup: light-dismiss closes the popup and keeps filters applied **(RG)**
- Sorting: `Title` / `Author` / `Date Added` work **(RG)**
- Sorting direction `▲/▼` toggles **(RG)**
- Sorting state persists across restart
- Covers: real cover keeps its aspect ratio
- Covers: placeholder letter is not clipped, including Cyrillic letters
- FB2 with `windows-1251` shows title/author without corruption **(RG)**

---

## Export

- Export: file appears on disk at the selected path **(RG)**
- Export: suggested filename contains no Windows-forbidden characters **(RG)**
- Export as EPUB: button is visible only when a converter is configured
- Compressed `.book.fb2.gz`: Export produces a readable `.fb2` **(RG)**
- Compressed `.book.fb2.gz`: Export as EPUB works **(RG)**
- Compressed `.book.fb2.gz`: Export as EPUB leaves no temp files in the library folder **(RG)**

---

## Delete

- Move to Recycle Bin: book disappears from the Library browser **(RG)**
- Move to Recycle Bin: files are removed from `Objects` **(RG)**
- If the last book of a language is deleted, that language disappears from the language filter **(RG)**

---

## Settings

- Open Library: path validation **(RG)**
- New Library: path validation **(RG)**
- Cyrillic path works in Settings **(RG)**
- Nested subfolder of the current library is rejected as a new root **(RG)**
- Converter configuration: relative path -> error **(RG)**
- Converter configuration: non-existent file -> error **(RG)**
- Converter configuration: working `fbc.exe` -> `Save` button becomes enabled **(RG)**
- Debounce ~500 ms — probe is not started on every keystroke
- After saving the converter path, the session reloads and stays on `Settings`
- Diagnostics: UI log, host log, and executable paths are visible
- About card: `VERSION` is not empty, `AUTHOR` and `CONTACT` are populated

---

## Regression

- Relaunch state persistence: Import preserves the last source path after restart
- Smoke pass after release build: `scripts\Run-Tests.ps1 -Preset x64-release -Configuration Release` passes **(RG)**
- Smoke launch release build: `scripts\Run-Librova.ps1 -Preset x64-release -Configuration Release` starts and opens the library **(RG)**
