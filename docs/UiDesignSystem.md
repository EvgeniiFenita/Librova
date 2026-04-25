# Librova UI Design System

Reference for everyone working on the Librova Qt/QML desktop UI.

---

## 1. Visual Language

**Style:** deep warm sepia with amber/gold accent; dark, focused, library-like.  
**Framework:** Qt/QML under `apps/Librova.Qt`.  
**Font:** Segoe UI Variable, Segoe UI, sans-serif.  
**Target OS:** Windows 11.

The design is quiet and work-focused: dense enough for managing a real library, but with generous spacing around reading metadata, filters, import progress, and destructive actions.

---

## 2. Colour Palette

Colour tokens live in `apps/Librova.Qt/qml/theme/LibrovaTheme.qml`. Do not hardcode hex values in views when a named token exists.

### Background layers

| Token | Hex | Usage |
|---|---|---|
| `background` | `#0D0A07` | Window root and title bar |
| `sidebar` | `#100C08` | Left navigation panel |
| `surface` | `#161108` | Main content area |
| `surfaceMuted` | `#1C160C` | Cards and default nav surfaces |
| `surfaceAlt` | `#221A0E` | Inputs and pointerover surfaces |
| `surfaceHover` | `#2A2012` | Button/control hover background |
| `surfaceElevated` | `#2E2414` | Popups, dropdowns, menus |

### Borders

| Token | Hex | Usage |
|---|---|---|
| `border` | `#2A200E` | Default 1 px borders |
| `borderStrong` | `#3A2E18` | Emphasis borders |
| `sidebarBorder` | `#211A0C` | Sidebar/content divider |

### Accent

| Token | Hex | Usage |
|---|---|---|
| `accent` | `#F5A623` | Primary accent and active borders |
| `accentBright` | `#FFD07A` | Hover on accent controls |
| `accentDim` | `#B87A1A` | Pressed accent |
| `accentMuted` | `#2A1C06` | Accent tint overlay |
| `accentSurface` | `#1A1003` | Accent panel background |
| `accentBorder` | `#3D2C0A` | Accent panel and popup border |

### Text

| Token | Hex | Usage |
|---|---|---|
| `textPrimary` | `#F5EDD8` | Default body text and active labels |
| `textSecondary` | `#A89880` | De-emphasized text |
| `textMuted` | `#967E68` | Placeholders, captions, inactive labels |

### Semantic status

| Token group | Surface | Border | Foreground |
|---|---|---|---|
| Success | `#08221A` | `#124E37` | `#34D39A` |
| Warning | `#221806` | `#4A3510` | `#F9BF4C` |
| Danger | `#200A0C` | `#551820` | `#F87171` |

---

## 3. Typography

Typography tokens live in `apps/Librova.Qt/qml/theme/LibrovaTypography.qml`.

| Role | Size | Weight | Usage |
|---|---|---|
| Display | 28 | Semibold | First-run title, major empty states |
| Section title | 22 | Semibold | Library, Import, Settings section headers |
| Card title | 16 | Semibold | Book cards and panel headings |
| Body | 14 | Regular | Main UI text |
| Caption | 12 | Regular | Supporting labels and diagnostics |
| Overline | 11 | Semibold | Uppercase group labels |

Do not scale font size with viewport width. Keep letter spacing at zero unless a specific token defines otherwise.

---

## 4. Shell Layout

The ready shell has:

- fixed left sidebar with brand, primary navigation, collections, current-library panel, and version label
- main content area with persistent `Library`, `Import`, and `Settings` sections
- section state preserved across navigation through `SectionLoader.qml`
- global file/folder drop handling that switches to `Import` and starts import immediately

Minimum desktop dimensions must preserve useful library browsing: at least two visible book-card columns and two visible rows when the details panel is open.

---

## 5. Navigation

Navigation controls use `LNavItem` and the sidebar collection item pattern.

- inactive nav label: `textMuted`
- active nav label: `textPrimary`
- active state: amber left border and warm active background
- disabled during active import where switching would break workflow state

Collections live under primary navigation. Collection entries use emoji glyphs from the approved set rendered with Segoe UI Emoji and a stable 30 x 30 icon area.

---

## 6. Controls

Shared controls live in `apps/Librova.Qt/qml/components/`.

| Component | Usage |
|---|---|
| `LButton` | Primary, secondary, destructive, and ghost actions |
| `LTextInput` | Search, path entry, and form fields |
| `LCheckBox` | Import covers, duplicate override, conversion options |
| `LNavItem` | Sidebar navigation and collection entries |
| `LToast` | Non-blocking status/error messages |

Toolbar controls with the same role must share height, background, border, and typography. Prefer minimum dimensions over fixed dimensions when localized or dynamic content may expand.

Disabled controls use uniform opacity reduction and retain their original chrome so the layout remains stable.

---

## 7. Icon System

Icons live in `apps/Librova.Qt/qml/theme/LibrovaIcons.qml` and are authored on a 24 x 24 grid. Use shared icon names instead of local path copies.

| Key | Shape | Usage |
|---|---|---|
| `book` | Bookmark silhouette | Sidebar brand |
| `library` | Books on shelf | Library nav |
| `import` | Arrow down into tray | Import nav/action |
| `exportIcon` | Arrow up from tray | Export action |
| `copy` | Two overlapping sheets | Copy title |
| `settings` | Gear | Settings nav |
| `search` | Magnifier | Search bar |
| `close` | Cross | Close/dismiss |
| `folderOpen` | Open folder | Open Library |
| `addFolder` | Folder plus | New Library / new collection |
| `trash` | Trash can | Delete/move to bin |
| `warning` | Triangle with exclamation | Validation errors |
| `check` | Checkmark | Success/complete |
| `refresh` | Circular arrow | Retry/reload |

---

## 8. Library Section

The Library section contains:

- toolbar with search, filters, sort, and direction controls
- responsive cover-card grid
- centered empty/no-results states with generated illustrations
- right details panel that opens without leaving the section
- book-card context menu for export, copy title, collection membership, and trash actions

Book cards keep stable dimensions. Real covers preserve aspect ratio on a neutral matte background. Missing covers use generated placeholders with readable initials, including Cyrillic letters.

The details panel must not overlap the grid. Opening or closing the panel recenters the selected card so selection remains visible after reflow.

---

## 9. Import Section

Import is immediate-start:

- dropping files/folders or selecting files/folders starts import without a separate Start button
- active import locks navigation/actions that would break job state
- progress shows total, processed, imported, failed, skipped, and current message
- cancellation exposes rolling back/compacting states instead of hiding them
- terminal result keeps aggregate summary and per-source warnings visible

`Import covers` is checked by default. `Force conversion to EPUB during import` is visible only when a converter is configured.

---

## 10. Settings Section

Settings is a single scrollable page with:

- converter executable path entry and browse action
- validation status and automatic preference save
- diagnostics paths for logs/runtime/converter/staging
- about/version information

Converter path validation runs after user edits, not as an eager probe on every startup.

---

## 11. First-Run And Recovery

First-run provides two explicit modes:

- create a new managed library in a new or empty folder
- open an existing complete managed library root

The left hero panel keeps the `Librova` wordmark prominent. The right panel owns mode selection, path input, validation, and continue action.

Startup recovery appears when the saved library root is invalid or damaged and lets the user choose a different library without silently recreating the broken one.

---

## 12. Title Bar

The title bar uses the same `background` colour as the window root. Windows-specific title-bar integration belongs in `QtWindowsPlatform`, not in QML views.

---

## 13. App Icon

Generated by `scripts/GenerateLibrovaIcon.py` into `apps/Librova.Qt/assets/librova.ico`.

Design: warm sepia rounded rectangle background with an amber/gold bookmark silhouette, soft glow, highlight strip, and spine strip.

Regenerate from the repo root:

```powershell
python scripts\GenerateLibrovaIcon.py apps\Librova.Qt\assets\librova.ico
```

---

## 14. Popup / Dropdown Surface Rule

All popup, dropdown, flyout, and context-menu surfaces use the same visual pair:

| Token | Value | Purpose |
|---|---|---|
| Background | `surfaceElevated` / `#2E2414` | Warm elevated surface, lighter than the main window |
| Border | `accentBorder` / `#3D2C0A` | Amber border that separates the container without relying on shadow |

Applied surfaces include filter popup, sort dropdown, book-card context menu, collection menu, and any submenu container. Any new popup or dropdown must follow this pair.

---

## 15. Section Transitions

Section transitions use short opacity fades around 150 ms. Transitions must not destroy section state; `Library`, `Import`, and `Settings` stay instantiated and toggle visibility.

---

## 16. Palette Change Checklist

To change the palette, update:

| File | What to change |
|---|---|
| `apps/Librova.Qt/qml/theme/LibrovaTheme.qml` | Primitive colours and derived tokens |
| `apps/Librova.Qt/App/QtWindowsPlatform.*` | Windows title-bar colour if the background token changes |
| `scripts/GenerateLibrovaIcon.py` | Icon colour constants, then regenerate `librova.ico` |
| `docs/UiDesignSystem.md` | Token table and any changed component rules |

Run `scripts\Run-Tests.ps1` after design-system changes that affect QML or icon assets.
