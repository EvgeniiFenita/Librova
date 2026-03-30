# Librova Manual UI Test Scenarios

This document is the manual test checklist for the current MVP UI.

Run the app through:

- `.\Run-Librova.ps1`

Before starting:

- keep [out\runtime\logs\ui.log](C:\Users\evgen\Desktop\Librova\out\runtime\logs\ui.log) available
- keep [out\runtime\library\Logs\host.log](C:\Users\evgen\Desktop\Librova\out\runtime\library\Logs\host.log) available

Use these scenarios after a meaningful UI or runtime change and before calling the app stable.

## 1. First Run Setup

1. Delete `out\runtime\ui-preferences.json` and `out\runtime\ui-shell-state.json`.
2. Run `.\Run-Librova.ps1`.
3. In the startup window, verify `Set up your Librova library` is shown.
4. Click `Continue` with an invalid relative path.
Expected:
- `Continue` is disabled or validation error is shown.
- host does not start.
5. Click `Browse...` and choose a valid empty folder.
6. Click `Continue`.
Expected:
- startup view changes to loading and then to the main application shell.
- [out\runtime\ui-preferences.json](C:\Users\evgen\Desktop\Librova\out\runtime\ui-preferences.json) is created.
- selected folder contains managed library structure such as `Database`, `Books`, `Covers`, `Temp`, `Logs`, `Trash`.

## 2. Startup Error Recovery

1. Close the app.
2. Edit `out\runtime\ui-preferences.json` so `PreferredLibraryRoot` points to a bad or inaccessible path.
3. Run `.\Run-Librova.ps1`.
Expected:
- startup error screen is shown.
- screen shows UI log, UI state, and preferences paths.
- screen also shows `Choose Another Library Root`.
4. Enter a valid folder and click `Retry With This Library`.
Expected:
- app starts successfully without manual file cleanup.
- the repaired library root is persisted for next launch.

## 3. Shell Navigation

1. In the left navigation, click `Library`.
Expected:
- header title becomes `Library`.
- only library content is visible in the main area.
2. Click `Import`.
Expected:
- header title becomes `Import`.
- only import content is visible.
3. Click `Settings`.
Expected:
- header title becomes `Settings`.
- only settings content is visible.
4. Scroll each section.
Expected:
- content scrolls vertically.
- panels do not overlap or render on top of each other.

## 4. Import Validation

1. Open the `Import` section.
2. Leave `Source File` empty.
Expected:
- `Start Import` is disabled.
3. Enter a non-existing absolute file path.
Expected:
- validation message says the source file does not exist.
- `Start Import` stays disabled.
4. Enter a real `.txt` file.
Expected:
- validation message says only `.fb2`, `.epub`, and `.zip` are supported.
5. Enter a valid `.fb2`, `.epub`, or `.zip` file and a valid working directory.
Expected:
- validation messages disappear.
- `Start Import` becomes enabled.

## 5. Import Workflow

1. In `Import`, choose a valid source file and valid working directory.
2. Click `Start Import`.
Expected:
- status changes from idle to running/completed text.
- `Start Import` disables while the job is active.
3. If the job is still running, click `Cancel`.
Expected:
- status reflects cancellation request or cancelled result.
4. Click `Refresh`.
Expected:
- current job state is queried without errors.
5. After a completed job, click `Remove Job`.
Expected:
- latest job id clears.
- summary/warnings/error area resets.

## 6. Drag And Drop Import Source

1. Open the `Import` section.
2. Drag a local `.fb2`, `.epub`, or `.zip` file onto the main window.
Expected:
- `Source File` updates to the dropped path.
- no crash or duplicate UI state appears.

## 7. Library Browser Basics

1. Open `Library`.
Expected:
- list loads automatically on app start or after switching to the section.
2. If books exist, click a book in the list.
Expected:
- `Selection Details` updates to the selected book.
3. With no books selected, inspect the details panel.
Expected:
- panel remains visible.
- it shows an empty-state message instead of blank or broken bindings.
4. Use search text and filters.
Expected:
- refresh returns matching items only.
5. Use `Next` and `Previous`.
Expected:
- page label updates.
- `Previous` disables on the first page.

## 8. Full Details Loading

1. In `Library`, select a book.
2. Click `Load Full Details`.
Expected:
- status updates while loading.
- extended metadata block fills with richer metadata such as publisher, ISBN, identifier, description, or SHA256 when available.

## 9. Export Book

1. In `Library`, select a book.
2. Click `Export Book...`.
3. Choose a destination path outside the managed library.
Expected:
- export completes successfully.
- destination file appears on disk.
- managed library source file remains untouched.

## 10. Move To Trash

1. In `Library`, select a book.
2. Click `Move To Trash`.
Expected:
- selected book disappears from the browser after refresh.
- file appears under the managed library `Trash` area.
- if the book had a cover, the cover is moved consistently too.

## 11. Import Updates Browser

1. Open `Library` and note the current book count.
2. Switch to `Import` and import a new valid book.
3. Return to `Library` if needed.
Expected:
- browser updates automatically after successful import.
- newly imported book appears without manual app restart.

## 12. Settings: Preferred Library Root

1. Open `Settings`.
2. Change `Next Launch Settings` library root to a different valid path.
3. Click `Save`.
Expected:
- status says settings were saved for next launch.
4. Restart the app.
Expected:
- app starts against the newly chosen library root.
5. Click `Reset` in settings and restart again.
Expected:
- app returns to default or current-session-based launch behavior.

## 13. Settings: Converter Configuration

1. Open `Settings`.
2. Select `BuiltInFb2Cng`.
3. Enter a relative executable path.
Expected:
- converter validation error is shown.
- save is disabled.
4. Enter an absolute executable path.
Expected:
- validation clears.
5. Switch to `CustomCommand`.
6. Enter an absolute executable path but leave arguments empty.
Expected:
- validation error says arguments are required.
7. Add one argument template per line and save.
Expected:
- settings save successfully for next launch.

## 14. Diagnostics And Logs

1. Open `Settings`.
2. Inspect `Diagnostics`.
Expected:
- UI log, host log, UI state, preferences, and host executable paths are shown.
3. Trigger a normal import and then inspect both log files.
Expected:
- UI log contains startup, command, and error-relevant entries.
- host log contains host/runtime-side operational entries.
- logs are useful and not dominated by tight polling noise.

## 15. Relaunch State Persistence

1. In `Import`, set a valid `Source File`, `Working Directory`, and `Allow probable duplicates`.
2. Close the app normally.
3. Run `.\Run-Librova.ps1` again.
Expected:
- import section restores the previous source path, working directory, and probable-duplicate toggle.

## 16. Smoke Pass After Release Build

1. Run:
   `.\Run-Tests.ps1 -Preset x64-release -Configuration Release`
2. Run:
   `.\Run-Librova.ps1 -Preset x64-release -Configuration Release`
Expected:
- release build completes.
- UI launches successfully from release artifacts.
- host resolves correctly from the `Release` layout.
