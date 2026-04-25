# Librova Release Checklist

Flat checklist for manual release verification. Items without `(RG)` are recommended checks; items marked `**(RG)**` are mandatory release gates.

---

## Startup & Navigation

- First-run wizard: Create New (empty folder) **(RG)**
- First-run wizard: Open Existing (valid library) **(RG)**
- First-run mode cards: hover state changes background and border **(RG)**
- First-run mode cards: selected state shows bright amber border **(RG)**
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
- Qt lifetime: the in-process backend closes cleanly when the UI exits **(RG)**

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
- Long-running import keeps progress visible and `Cancel` available
- Cancellation: rollback of already imported books **(RG)**
- Cancellation: the library returns to the pre-import state **(RG)**
- Drag-and-drop: files start import automatically **(RG)**
- Drag-and-drop: folder starts import automatically **(RG)**
- Drag-and-drop: multiple files at once
- After import completes, the Library browser refreshes automatically **(RG)**
- Duplicate override: `Allow probable duplicates` creates a separate record only for probable duplicates; strict duplicates stay rejected **(RG)**
- `Force conversion to EPUB during import` is visible only when a converter is configured
- `Import covers` checkbox is checked by default; when unchecked, imported books have no cover thumbnail and no cover file is stored

---

## Library Browser

- Book grid is visible and infinite scroll works **(RG)**
- Empty library: book illustration + "Go to Import" button are shown **(RG)**
- No-results state: search or filter returns 0 books — magnifier illustration + "Nothing found" shown, no button **(RG)**
- No flicker when clearing the search field: empty state does not flash between states
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
- `Esc` with a selected book closes the details panel (clears selection); does nothing when no book is selected
- `Delete` with a selected book moves the book to the Recycle Bin; does nothing when no book is selected
- `Delete` in the search field deletes text and does not trigger the trash command
- Right-click on a non-selected book card selects that card before opening the context menu
- Book-card context menu exposes `Export`, `Copy Title`, and `Move to Trash`
- With a configured converter, book-card context menu also exposes `Export as EPUB`; it is enabled only for `FB2` books
- Collections: sidebar shows emoji-icon collection buttons under Library/Import; empty state has 📚 glyph + "Create one" CTA; "New collection" ghost button appears below the list when collections exist; delete is via right-click context menu on a collection item
- Collections: creating a collection adds it to the sidebar without leaving `Library` **(RG)**
- Collections: selecting a collection filters the ordinary library grid to that collection **(RG)**
- Collections: clicking `Library` clears the active collection view and returns to the full catalog
- Collections: book-card context menu exposes `Add to` submenu with existing collections
- Collections: `Create new...` from the book-card context menu creates the collection and adds that book to it **(RG)**
- Collections: `Remove from collection` is visible in collection view and removes only membership, not the book itself **(RG)**
- Collections: deleting a collection keeps its books in the library and removes only memberships **(RG)**
- Book details: selected-book details panel shows collection memberships when present
- Collection view: drag-and-drop import is disabled while a collection is active

---

## Export

- Export: file appears on disk at the selected path **(RG)**
- Export: suggested filename contains no Windows-forbidden characters **(RG)**
- Export as EPUB: button is visible only when a converter is configured
- Export as EPUB: without a configured converter, an `FB2` details panel hides the action and shows the `Settings` guidance instead
- Export as EPUB: converter failure after a partial write does not corrupt an existing destination file
- Compressed `.book.fb2.gz`: Export produces a readable `.fb2` **(RG)**
- Compressed `.book.fb2.gz`: Export as EPUB works **(RG)**
- Compressed `.book.fb2.gz`: Export as EPUB leaves no temp files in the library folder **(RG)**

---

## Delete

- Move to Recycle Bin: book disappears from the Library browser **(RG)**
- Move to Recycle Bin: files are removed from `Objects` **(RG)**
- Move to Recycle Bin: partial Windows handoff failure is reported as a managed-trash fallback
- If the last book of a language is deleted, that language disappears from the language filter **(RG)**

---

## Settings

- Open Library: path validation **(RG)**
- New Library: path validation **(RG)**
- Cyrillic path works in Settings **(RG)**
- Nested subfolder of the current library is rejected as a new root **(RG)**
- Converter configuration: relative path -> error **(RG)**
- Converter configuration: non-existent file -> error **(RG)**
- Converter configuration: working `fbc.exe` is accepted and saved automatically **(RG)**
- Debounce ~500 ms — probe is not started on every keystroke
- After saving the converter path, the session reloads and stays on `Settings`
- Diagnostics: UI log, runtime workspace, converter workspace, staging path, and executable/runtime paths are visible
- About card: `VERSION` is not empty, `AUTHOR` and `CONTACT` are populated

---

## Regression

- Relaunch state persistence: Import preserves the last source path after restart
- Smoke pass after release build: `scripts\Run-Tests.ps1 -Preset x64-release -Configuration Release` passes **(RG)**
- Smoke launch release build: `scripts\Run-LibrovaQt.ps1 -Preset x64-release -Configuration Release` starts and opens the library **(RG)**
