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

- inactive nav label: `textMuted`; background: `surfaceAlt`; border always visible (`border` token)
- active nav label: `textPrimary`
- active state: amber left border and warm active background
- content indent is fixed (`sp3`) regardless of active state so icon and text never jump
- disabled during active import where switching would break workflow state

Collections live under primary navigation. Collection entries use emoji glyphs from the approved set rendered with Segoe UI Emoji and a stable 20 px icon slot.

---

## 6. Controls

Shared controls live in `apps/Librova.Qt/qml/components/`.

| Component | Usage |
|---|---|
| `LButton` | Primary, secondary, accent, and destructive actions |
| `LTextInput` | Search, path entry, and form fields |
| `LCheckBox` | Import covers, duplicate override, conversion options |
| `LNavItem` | Sidebar navigation and collection entries |
| `LToast` | Non-blocking status/error messages |
| `LScrollBar` | Styled scrollbar for `Flickable`/`GridView`; autohide, warm amber handle |

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

### Hero panel (left column)

The left column shows the app brand: `Image` from `qrc:/assets/librova_hero.png` at 140×140 (`fillMode: PreserveAspectFit`), then "LIBROVA" in uppercase with `font.letterSpacing: LibrovaTypography.spacingBrand` (4.0), then "YOUR BOOKS. ORGANIZED." in uppercase with `font.letterSpacing: 1.2` in `textMuted`. The panel background is `accentSurface`.

### Mode cards (right column)

Two side-by-side `Rectangle` cards — Create New and Open Existing. Interaction states:

| State | Background | Border |
|---|---|---|
| Default | `surface` | `border` 1 px |
| Hover | `surfaceHover` | `borderStrong` 1 px |
| Selected | `accentSurface` | `accent` (#F5A623) 2 px |

Hover is driven by `HoverHandler.hovered` bound directly in the color expression. Do **not** use `HoverHandler.containsMouse` — that property does not exist on `HoverHandler` and always evaluates to `undefined`, so hover colors never apply. The indirect `_hov` boolean property pattern is legacy; prefer direct `.hovered` binding.

Both `color` and `border.color` animate with `Behavior { ColorAnimation { duration: LibrovaTheme.animFast } }`. `border.width` is **not** animated (1 → 2 px transition causes blur artifacts).

### Path input and Browse

The path row uses a `Row` with `spacing: 8`: an `LTextInput` fills available width, followed by an `LButton` with `variant: "secondary"` labelled "Browse" at fixed width 120 px. The two controls are visually independent — no shared focus border.

### Startup recovery

`StartupRecoveryView.qml` uses the same path+browse `Row` pattern as the first-run path row.

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

## 15. Scrollbar

Use `LScrollBar` (`qml/components/LScrollBar.qml`) for all scrollable content areas. The component is 5 px wide, autohides when the content fits or is idle, and follows the warm amber palette.

**Attach only to `Flickable` or its subclasses** (`GridView`, `ListView`). Do not attach to `ScrollView` — `ScrollBar.vertical` is not properly forwarded by Qt Quick's `ScrollView` template to custom `ScrollBar` subclasses. Replace `ScrollView` + content item with `Flickable` + `contentHeight: _content.implicitHeight` instead.

Scrollbars are enabled in: `BookGrid` (GridView), `BookDetailsPanel` (Flickable), and the filters popup genre list (Flickable). Other views do not need scrollbars.

---

## 16. Split-Pill Button Hover Radius

When a pill-shaped control is divided into two interactive halves by a 1 px divider (e.g. the sort pill: label + direction-icon), each half's hover background must use per-corner radius instead of a uniform `radius`:

- Left half: `topLeftRadius` + `bottomLeftRadius` = `LibrovaTheme.radiusMedium`; right corners = 0
- Right half: `topRightRadius` + `bottomRightRadius` = `LibrovaTheme.radiusMedium`; left corners = 0

This requires Qt 6.7+. Librova targets Qt 6.11, so these properties are always available.

Note: `clip: true` on a `Rectangle` clips children to the bounding box, not to the rounded-corner shape. Do not rely on the parent's clip to round child hover backgrounds.

---

## 17. Section Transitions

Section transitions use short opacity fades around 150 ms. Transitions must not destroy section state; `Library`, `Import`, and `Settings` stay instantiated and toggle visibility.

---

## 18. Palette Change Checklist

To change the palette, update:

| File | What to change |
|---|---|
| `apps/Librova.Qt/qml/theme/LibrovaTheme.qml` | Primitive colours and derived tokens |
| `apps/Librova.Qt/App/QtWindowsPlatform.*` | Windows title-bar colour if the background token changes |
| `scripts/GenerateLibrovaIcon.py` | Icon colour constants, then regenerate `librova.ico` |
| `docs/UiDesignSystem.md` | Token table and any changed component rules |

Run `scripts\Run-Tests.ps1` after design-system changes that affect QML or icon assets.
