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

The enforced minimum window size is **1300 × 1040 px** (set in `Main.qml`):

- `minimumWidth: 1300` — sidebar (244 px) + 1 px divider + grid with 3 card columns (3 × 216 px + 32 px margins) + 1 px divider + details panel (360 px) + scrollbar (~12 px)
- `minimumHeight: 1040` — tall enough to show the import section (header + drop zone 340 px + options + progress card) without scrolling on FHD displays (1080 px − ~40 px taskbar)

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

The sidebar brand badge uses `qrc:/assets/brand_badge.png` (112 × 112 px source, displayed at 56 × 56 logical px with `mipmap: true`), not a vector icon. Do not use `LIcon` for the brand slot.

---

## 8. Library Section

The Library section contains:

- toolbar with search, filters, sort, and direction controls
- responsive cover-card grid
- centered empty/no-results states with illustrated assets
- right details panel that opens without leaving the section
- book-card context menu for export, copy title, collection membership, and trash actions

Book cards keep stable dimensions (`188 x 330` delegates in `216 x 352` grid cells). Real covers preserve aspect ratio and sit directly on the card surface without a visible inner backing frame. Missing or failed covers use rounded deterministic warm gradient placeholders with readable initials, including Cyrillic letters.

The selected state belongs to the full card, not the cover slot: the amber ring stays above the card content and must never be hidden by the cover image. Card title text uses up to two lines with right-side ellipsis; author text uses one muted line with ellipsis.

The details panel must not overlap the grid. When the details panel opens and the selected card is already fully visible, the grid does not scroll. If the selected card is outside the visible area, the grid scrolls the minimum amount to bring it into view.

### Book Details Panel

`BookDetailsPanel.qml` is a 360 px fixed-width side panel. Layout (top to bottom):

1. **Header row** — book title (full width) + close button pinned top-right. Close button: 28×28 px, `radius: 6`, `surfaceMuted` background with `border` border, `LibrovaIcons.close` icon (14 px), hover animation via `surfaceHover`.
2. **Two-column row** — cover image (150×210 px) left; authors + Year / Language / Format / Size right. Authors truncate to 2 lines with right-side ellipsis; hovering a truncated author row shows a `ToolTip` (width 280, padding 10) with each author on its own line.
3. **Full-width metadata** — ISBN and Series side by side in equal columns when both are present; single column at full width when only one is present. Publisher below in full width.
4. **Genres** — plain non-interactive `Rectangle` pills (no hover, no pointer cursor). Never rendered as `LGenreChip`.
5. **Annotation** — dynamic-height block. When content is small the block sizes to the content. When content is large the block expands to fill all remaining panel space and a `Flickable` with `LScrollBar` scrolls the text. The panel itself does **not** scroll — only the annotation `Flickable` does.
6. **Action buttons** — pinned to the bottom by a spacer above. Three equal stacked buttons (full panel width, 46 px height, `sizeMd` font, 22 px icons): `Export as EPUB` (accent colour, visible only when converter is configured), `Export` (neutral), `Move to Recycle Bin` (danger palette). All three are disabled (opacity 0.4) while `exportAdapter.isBusy`. Hover and icon/text color effects are suppressed while busy.

Cover placeholder follows the same 7-palette deterministic warm-gradient rule as `BookCard.qml` (hash seeded by `title + "|" + authors`). The book database ID is never shown in the panel.

### Empty and No-Results State Illustrations

The Library section uses three pre-rendered PNG illustration assets:

| State | Asset | Physical size | Display size | Component |
|---|---|---|---|---|
| Library is empty | `assets/empty_library.png` | 440 × 300 px RGBA | 220 × 150 logical px | `EmptyBookIllustration.qml` |
| Search/filter yields no results | `assets/no_results.png` | RGBA | same ratio | inline `Image` in `BookGrid.qml` |

All assets use `mipmap: true` and transparent backgrounds that blend onto the `surface` token.

To replace an illustration: swap the PNG file (keep physical dimensions for crisp 2× HiDPI rendering) and rebuild — no QML changes required.

### Sidebar Collections Empty State

When no collections exist, the sidebar panel shows `assets/no_collections.png` (400 × 260 px source, displayed at 100 × 65 logical px with `mipmap: true`) in place of a text/emoji placeholder.

---

## 9. Import Section

Import is immediate-start:

- dropping files/folders or selecting files/folders starts import without a separate Start button
- active import locks navigation/actions that would break job state
- progress shows total, processed, imported, failed, skipped, and current message
- cancellation exposes rolling back/compacting states instead of hiding them
- terminal result keeps aggregate summary and per-source warnings visible

`Import covers` is checked by default. `Force conversion to EPUB during import` is visible only when a converter is configured.

### Drop Zone Illustration

The idle drop zone card (`height: 340 px`) shows `assets/import_drop.png` (560 × 280 px source, displayed at 280 × 140 logical px with `mipmap: true`) centered above the "Drop files or folders here" label and action buttons.

---

## 10. Settings Section

Settings is a single scrollable page with:

- converter executable path entry and browse action
- validation status and automatic preference save
- diagnostics paths for logs/runtime/converter/staging
- about/version information

Converter path validation runs after user edits, not as an eager probe on every startup.

The About card displays `assets/brand_badge.png` (112 × 112 px source, displayed at 48 × 48 logical px with `mipmap: true`) as the app logo.

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

Three DWM attributes are applied in `QtWindowsPlatform::setWindow()`:

| Attribute | Value | Purpose |
|---|---|---|
| `DWMWA_USE_IMMERSIVE_DARK_MODE` | `TRUE` | Dark chrome fallback (Windows 10 / 11) |
| `DWMWA_CAPTION_COLOR` | `RGB(0x0D, 0x0A, 0x07)` = `#0D0A07` | Caption background matches `background` token |
| `DWMWA_TEXT_COLOR` | `RGB(0xF5, 0xED, 0xD8)` = `#F5EDD8` | Caption text matches `textPrimary` token |

`DWMWA_CAPTION_COLOR` and `DWMWA_TEXT_COLOR` require Windows 11 (build 22000+); the calls silently fail on Windows 10, leaving the dark-mode chrome as the fallback.

---

## 13. App Icon

Asset: `apps/Librova.Qt/assets/librova.ico` — 7 sizes (16, 24, 32, 48, 64, 128, 256 px).

Design: dark rounded-square background with an amber/gold glowing open book. To replace the icon, generate a new source PNG (1024×1024 or larger, with dark rounded-square background, no transparent corners), then convert to `.ico` using Pillow:

```python
from PIL import Image
img = Image.open("source.png").convert("RGB")
sizes = [256, 128, 64, 48, 32, 24, 16]
frames = [img.resize((s, s), Image.LANCZOS) for s in sizes]
frames[0].save("apps/Librova.Qt/assets/librova.ico",
               format="ICO", sizes=[(s,s) for s in sizes],
               append_images=frames[1:])
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

Scrollbars are enabled in: `BookGrid` (GridView), the annotation `Flickable` inside `BookDetailsPanel` (annotation block only — the panel itself does not scroll), and the filters popup genre list (Flickable). Other views do not need scrollbars.

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
| `apps/Librova.Qt/assets/empty_library.png` | Empty library state illustration (440 × 300 px RGBA PNG) |
| `apps/Librova.Qt/assets/no_results.png` | No-results state illustration (same ratio, RGBA PNG) |
| `apps/Librova.Qt/assets/no_collections.png` | Sidebar collections empty state (400 × 260 px RGBA PNG) |
| `apps/Librova.Qt/assets/import_drop.png` | Import drop zone illustration (560 × 280 px RGBA PNG) |
| `apps/Librova.Qt/assets/brand_badge.png` | Sidebar brand badge and About card logo (112 × 112 px RGBA PNG) |
| `apps/Librova.Qt/App/QtWindowsPlatform.*` | Windows title-bar colour if the background token changes |
| `apps/Librova.Qt/assets/librova.ico` | Replace with a new PNG converted via Pillow (see §13) |
| `docs/UiDesignSystem.md` | Token table and any changed component rules |

Run `scripts\Run-Tests.ps1` after design-system changes that affect QML or icon assets.
