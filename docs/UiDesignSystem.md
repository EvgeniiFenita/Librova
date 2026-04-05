# Librova UI Design System

> Reference for everyone working on `apps/Librova.UI`.  
> All source of truth is in `Styles/Colors.axaml`, `Styles/Components.axaml`, `Styles/Typography.axaml`.

---

## 1. Visual Language

**Style:** Deep midnight navy + electric sky-blue accent. Modern, dark, low-noise.  
**Framework:** Avalonia 11 + FluentTheme (dark variant).  
**Font:** `Segoe UI Variable, Segoe UI, sans-serif` — set globally on `Window`.  
**Target OS:** Windows 11. DWM title-bar colour is set via P/Invoke (`DWMWA_CAPTION_COLOR`).

---

## 2. Colour Palette

All colours live in `Colors.axaml` as `Color` primitives + `SolidColorBrush` resources.  
**Never hardcode hex values in views or styles — always use a `{DynamicResource}` brush.**

### Background layers (darkest → lightest)

| Token | Hex | Usage |
|---|---|---|
| `AppBackgroundBrush` | `#07090F` | Window root, title bar |
| `AppSidebarBrush` | `#0A0F1D` | Left navigation panel |
| `AppSurfaceBrush` | `#111827` | Main content area |
| `AppSurfaceMutedBrush` | `#162032` | Cards, NavItem default bg |
| `AppSurfaceAltBrush` | `#1B2840` | Inputs, pointerover surfaces |
| `AppSurfaceHoverBrush` | `#223050` | Button / control hover bg |

### Borders

| Token | Hex | Usage |
|---|---|---|
| `AppBorderBrush` | `#1C2C42` | Default 1 px borders |
| `AppBorderStrongBrush` | `#2C4060` | Emphasis borders |
| `AppSidebarBorderBrush` | `#16233A` | Sidebar / content divider |

### Accent (electric sky-blue)

| Token | Hex | Usage |
|---|---|---|
| `AppAccentBrush` | `#4CC2FF` | Primary accent, active borders |
| `AppAccentBrightBrush` | `#99DEFF` | Hover on accent controls |
| `AppAccentDimBrush` | `#2A8AB8` | Pressed accent |
| `AppAccentMutedBrush` | `#0F2D45` | Accent tint overlay |
| `AppAccentSurfaceBrush` | `#081E30` | Accent panel background |
| `AppAccentBorderBrush` | `#1A4A6A` | Accent panel border |

### Navigation active state

| Token | Hex | Usage |
|---|---|---|
| `AppNavActiveBrush` | `#0E2A48` | Active NavItem background |
| `AppNavActiveHoverBrush` | `#12305A` | Active NavItem hover |

### Text hierarchy

| Token | Hex | Usage |
|---|---|---|
| `AppTextPrimaryBrush` | `#EAF0FA` | Default body text, active nav label |
| `AppTextSecondaryBrush` | `#8AA3BF` | De-emphasised text |
| `AppTextMutedBrush` | `#7899B6` | Placeholder, captions, inactive nav label |

### Semantic status

| Token | Surface | Border | Foreground |
|---|---|---|---|
| Success | `AppSuccessSurfaceBrush` `#08221A` | `AppSuccessBorderBrush` `#124E37` | `AppSuccessBrush` `#34D39A` |
| Warning | `AppWarningSurfaceBrush` `#221806` | — | `AppWarningBrush` `#F9BF4C` |
| Danger | `AppDangerSurfaceBrush` `#200A0C` | `AppDangerBorderBrush` `#551820` | `AppDangerBrush` `#F87171` |
| Danger text | — | — | `AppDangerTextBrush` `#FFDDDD` / `AppDangerTextHoverBrush` `#FFE8E8` |
| Danger hover bg | `AppDangerHoverBgBrush` `#3D0E12` | — | — |

### Specialised

| Token | Hex | Usage |
|---|---|---|
| `AppOnAccentBrush` | `#04101A` | Text on filled accent (PrimaryAction) button |
| `AppMatteBrush` | `#040710` | Book cover placeholder background |
| `AppCoverPlaceholderBrush` | `#DDE8F8` | Initial letter on cover placeholder |

---

## 3. Spacing & Shape Tokens

Defined in `Colors.axaml`.

### Corner radius

| Token | Value | Usage |
|---|---|---|
| `Radius.Small` | 8 | NavItem buttons, IconAction |
| `Radius.Medium` | 12 | Inputs, ComboBox, BookCard, secondary panels |
| `Radius.Large` | 18 | Section panels (AccentPanel, SuccessPanel, DangerPanel) |
| `Radius.XLarge` | 24 | Top-level panels (AppPanel, AppShell) |

### Padding

| Token | Value | Usage |
|---|---|---|
| `Padding.Card` | 22 | Full panels (AppPanel, DangerPanel) |
| `Padding.CardCompact` | 16 | Compact panels, AppPanelFlat |
| `Padding.Sidebar` | 16,20 | Sidebar outer padding |

**Base grid unit = 4 px.** All spacing and margin values should be multiples of 4.

---

## 4. Typography

Defined in `Typography.axaml`. Base size 14 px, line-height 22 px, `AppTextPrimaryBrush`.

| Class | Size | Weight | Colour | Usage |
|---|---|---|---|---|
| *(none)* | 14 | Regular | Primary | Default body |
| `DisplayTitle` | 32 | Bold | Primary | Startup / error hero headings |
| `ViewTitle` | 22 | SemiBold | Primary | Page title (Library, Import, Settings) |
| `SectionTitle` | 16 | SemiBold | Primary | Card / panel heading |
| `SectionEyebrow` | 11 | Bold, +1.4 spacing | Accent | Category overline (LIBRARY, RUNNING …) |
| `BodyMuted` | 14 | Regular | Secondary | Supporting descriptions |
| `Caption` | 12 | Regular | Muted | Paths, hints, small labels |
| `MetaLabel` | 11 | Bold, +0.6 spacing | Muted | Metadata key labels (AUTHOR, YEAR …) |

---

## 5. Panel / Surface Components

Defined in `Components.axaml`.

| Class | Radius | Padding | Background | Use case |
|---|---|---|---|---|
| `AppPanel` | XLarge | Card | Surface | Top-level content cards |
| `AppPanelCompact` | Large | CardCompact | Surface | Mid-level cards |
| `AppPanelMuted` | Medium | CardCompact | SurfaceMuted | Inset info blocks |
| `AppPanelFlat` | 0 | CardCompact | Surface | Full-bleed content areas (book grid) |
| `AccentPanel` | Large | CardCompact | AccentSurface | Highlighted action sections |
| `SuccessPanel` | Large | CardCompact | SuccessSurface | Completed operation results |
| `DangerPanel` | Large | Card | DangerSurface | Error / startup failure screens |
| `DropZone` | Large | 32,40 | AccentSurface | Drag-and-drop target |
| `MetricPill` | Medium | 14,8 | SurfaceAlt | Book count badge |
| `CoverMatte` | 12 (hard) | — | Matte | Book cover rounded frame |

---

## 6. Button Classes

All buttons inherit the global `Button` style: `Cursor=Hand`, background transition 120 ms.

| Class | Appearance | Use case |
|---|---|---|
| `PrimaryAction` | Filled accent, dark text, SemiBold, h=42 | Main CTA (Continue, Save, Select Files) |
| `SecondaryAction` | SurfaceAlt bg, border, h=42 | Secondary actions (Browse, Cancel, Open) |
| `AccentAction` | AccentSurface bg, accent border, h=42 | Accent-tinted actions (Export as EPUB) |
| `DestructiveAction` | DangerSurface bg, danger border, SemiBold | Destructive ops (Move to Recycle Bin) |
| `IconAction` | SurfaceAlt bg, border, 40×40, square | Icon-only standard size |
| `IconActionSm` | SurfaceAlt bg, border, 36×36, square | Icon-only compact (panel headers) |
| `NavItem` | SurfaceMuted bg, border, h=42, Radius.Small | Sidebar nav (Library, Import, Settings) |
| `NavItem` + `.Active` | NavActive bg, accent border | Currently selected nav section |

### Nav button text behaviour
- Inactive → `TextBlock.NavLabel` Foreground = `AppTextMutedBrush`
- Active → `TextBlock.NavLabel` Foreground = `AppTextPrimaryBrush`  
- Disabled (import in progress) → whole button `Opacity=0.45`

### Disabled states
Every button class has explicit `:disabled /template/ ContentPresenter` overrides that set `Background=SurfaceMuted`, `Foreground=TextMuted`. This prevents FluentTheme from applying its own dimming on top.

---

## 7. Icon System

Icons are `StreamGeometry` resources defined in `Colors.axaml`.  
Rendered via `PathIcon` — fills all sub-paths as solid colour.

| Key | Shape | Usage |
|---|---|---|
| `IconBook` | Bookmark silhouette | Sidebar brand |
| `IconLibrary` | Books on shelf | Library nav |
| `IconImport` | Arrow DOWN into tray | Import nav / action |
| `IconExport` | Arrow UP from tray | Export action |
| `IconSettings` | Gear | Settings nav |
| `IconSearch` | Magnifier | Search bar |
| `IconClose` | × cross | Close/dismiss |
| `IconFolderOpen` | Open folder | Open Library |
| `IconAddFolder` | Folder + plus | New Library |
| `IconTrash` | Trash can | Delete / move to bin |
| `IconInfo` | Info circle | Info hint |
| `IconUploadCloud` | Cloud with arrow | Drop zone |
| `IconChevronRight` | › chevron | Navigation hint |

### PathIcon classes

| Class | Size | Colour | Usage |
|---|---|---|---|
| `AppIcon` | 18×18 | TextSecondary | Default icon in controls |
| `AppIconAccent` | 18×18 | Accent | Highlighted / action icons |
| `Button.NavItem.Active PathIcon.AppIcon` | — | Accent | Active nav icon override |

---

## 8. Form Controls

| Control | Class | Notes |
|---|---|---|
| `TextBox` | `AppTextInput` | h=42, SurfaceAlt bg, Medium radius |
| `TextBox` | `AppTextArea` | Multi-line, TextWrapping=Wrap |
| `ComboBox` | `AppComboBox` | Popup styled via `Border#PopupBorder` override |
| `CheckBox` | *(none)* | Foreground override to Primary |

### ScrollBar
Styled entirely via FluentTheme resource key overrides in `Colors.axaml` (no `/template/` selectors except CornerRadius=4 on Thumb). Key states:
- Collapsed: `ScrollBarPanningThumbBackground` = `BorderStrong`  
- Expanded: `ScrollBarThumbBackgroundColor` = `BorderStrong`  
- Hover: `ScrollBarThumbFillPointerOver` = Accent  
- Pressed: `ScrollBarThumbFillPressed` = AccentDim

---

## 9. Shell Layout

```
Window (Background = AppBackgroundBrush)
└── Grid
    ├── [IsStartingUp]    Border — full-bleed startup screen
    ├── [IsFirstRun]      Border — setup wizard
    ├── [HasStartupError] Border — recovery screen
    └── [HasShell]        Grid  ColumnDefinitions="244,*"
        ├── Col 0: Border (AppSidebarBrush) — sidebar fill
        │          Rectangle 1px — divider
        │          Border Padding="14,18" — sidebar content
        │              DockPanel
        │                  Bottom: library card + Settings NavItem
        │                  Fill:   branding + Library/Import NavItems
        └── Col 1: Border (AppSurfaceBrush) — content fill
                   Grid (DataContext=Shell)
                       LibraryView / ImportView / SettingsView
```

**Sidebar width:** 244 px (fixed).  
**Content column:** fills remaining space (`*`).

---

## 10. View Structure

| View | DataContext | Key layout |
|---|---|---|
| `LibraryView` | `ShellViewModel` | Grid: toolbar (Auto) + content row (*). Content = book grid (AppPanelFlat) + details panel (360 px fixed, right) |
| `ImportView` | `ShellViewModel` | ScrollViewer → StackPanel. Drop zone → options → running state → result |
| `SettingsView` | `ShellViewModel` | ScrollViewer → StackPanel. Converter → About → Diagnostics panels |

---

## 11. Disabled / Lock-out State

During import (`IsImportInProgress = true`) the UI is locked:

- `ShowLibrarySectionCommand`, `ShowImportSectionCommand`, `ShowSettingsSectionCommand` → `CanExecute = false` → `NavItem:disabled` → `Opacity=0.45`
- `TextBox:disabled`, `ComboBox:disabled`, `CheckBox:disabled` → `Opacity=0.38`
- `Border.AppPanelMuted:disabled` → `Opacity=0.45`
- Buttons: explicit `:disabled /template/ ContentPresenter` styles dim bg/fg
- Library card in sidebar (`Border.AppPanelMuted`) — `IsEnabled` bound to `!Shell.IsImportInProgress`; dims to 0.45 opacity and prevents Open/New button interaction
- OPTIONS panel in `ImportView` (`Border.AppPanelMuted`) — `IsEnabled` bound to `!ImportJobs.IsBusy`; dims both checkboxes (Allow duplicate import, Force conversion to EPUB) via parent propagation
- Only the "Cancel" button remains active

> **Important:** Nav commands must NOT use `CurrentSection is not ShellSection.X` in CanExecute — that would disable the active button and break active-state styling.

---

## 12. Title Bar

Controlled via DWM P/Invoke (`DWMWA_CAPTION_COLOR = 35`, Windows 11 only).  
Colour: `#07090F` as COLORREF `0x000F0907`.  
Set in `MainWindow.axaml.cs → OnOpened`. Catches both `DllNotFoundException` and `EntryPointNotFoundException` for cross-version safety.

---

## 13. App Icon

Generated by `scripts/GenerateLibrovaIcon.py` → `apps/Librova.UI/Assets/librova.ico`.  
Design: navy rounded rectangle background + electric cyan bookmark silhouette with soft glow, highlight strip, spine strip.  
Regenerate: `python scripts\GenerateLibrovaIcon.py apps\Librova.UI\Assets\librova.ico`
