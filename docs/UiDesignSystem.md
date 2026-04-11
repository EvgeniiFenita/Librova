# Librova UI Design System

> Reference for everyone working on `apps/Librova.UI`.  
> All source of truth is in `Styles/Colors.axaml`, `Styles/Components.axaml`, `Styles/Typography.axaml`.

---

## 1. Visual Language

**Style:** Deep warm sepia + amber/gold accent. Dark, cosy, library-like.  
**Framework:** Avalonia 12 + FluentTheme (dark variant).  
**Font:** `Segoe UI Variable, Segoe UI, sans-serif` — set globally on `Window`.  
**Target OS:** Windows 11. DWM title-bar colour is set via P/Invoke (`DWMWA_CAPTION_COLOR`).

---

## 2. Colour Palette

All colours live in `Colors.axaml` as `Color` primitives + `SolidColorBrush` resources.  
**Never hardcode hex values in views or styles — always use a `{DynamicResource}` brush.**
For C# presentation helpers that cannot bind XAML resources directly, resolve shared brushes through `apps/Librova.UI/Styling/UiThemeResources.cs` so code stays aligned with the same named tokens and fallback values.

### Background layers (darkest → lightest)

| Token | Hex | Usage |
|---|---|---|
| `AppBackgroundBrush` | `#0D0A07` | Window root, title bar |
| `AppSidebarBrush` | `#100C08` | Left navigation panel |
| `AppSurfaceBrush` | `#161108` | Main content area |
| `AppSurfaceMutedBrush` | `#1C160C` | Cards, NavItem default bg |
| `AppSurfaceAltBrush` | `#221A0E` | Inputs, pointerover surfaces |
| `AppSurfaceHoverBrush` | `#2A2012` | Button / control hover bg |

### Borders

| Token | Hex | Usage |
|---|---|---|
| `AppBorderBrush` | `#2A200E` | Default 1 px borders |
| `AppBorderStrongBrush` | `#3A2E18` | Emphasis borders |
| `AppSidebarBorderBrush` | `#211A0C` | Sidebar / content divider |

### Accent (amber / gold)

| Token | Hex | Usage |
|---|---|---|
| `AppAccentBrush` | `#F5A623` | Primary accent, active borders |
| `AppAccentBrightBrush` | `#FFD07A` | Hover on accent controls |
| `AppAccentDimBrush` | `#B87A1A` | Pressed accent |
| `AppAccentMutedBrush` | `#2A1C06` | Accent tint overlay |
| `AppAccentSurfaceBrush` | `#1A1003` | Accent panel background |
| `AppAccentBorderBrush` | `#3D2C0A` | Accent panel border |

### Navigation active state

| Token | Hex | Usage |
|---|---|---|
| `AppNavActiveBrush` | `#1E1606` | Active NavItem background |
| `AppNavActiveHoverBrush` | `#261C08` | Active NavItem hover |

### Text hierarchy

| Token | Hex | Usage |
|---|---|---|
| `AppTextPrimaryBrush` | `#F5EDD8` | Default body text, active nav label |
| `AppTextSecondaryBrush` | `#A89880` | De-emphasised text |
| `AppTextMutedBrush` | `#967E68` | Placeholder, captions, inactive nav label |

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
| `AppOnAccentBrush` | `#0A0700` | Text on filled accent (PrimaryAction) button |
| `AppMatteBrush` | `#0A0700` | Book cover placeholder background |
| `AppCoverPlaceholderBrush` | `#F5EDD8` | Initial letter on cover placeholder |
| `AppSidebarGradientBrush` | `#0A0806` → `#1A1409` | Sidebar vertical gradient (see §16) |

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
| `MetricPillAccent` | Medium | 14,8 | AccentSurface + AccentBorder | Accent-tinted stat pill (book count in toolbar) |
| `SortGroup` | Medium | — | SurfaceAlt + Border, `ClipToBounds=True` | Unified capsule wrapping sort key ComboBox + direction button; child `ComboBox.AppComboBox` and `Button.IconAction` lose their own borders inside this container |
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
Every action button class (`AccentAction`, `SecondaryAction`, `DestructiveAction`) uses two rules when disabled:
- `Button.*:disabled { Opacity = 0.4 }` — dims the entire button uniformly, including icons with explicit `Foreground` bindings.
- `Button.*:disabled /template/ ContentPresenter { Background = <original-enabled-bg>, Foreground = TextMuted }` — keeps the button shape visible and mutes inherited text colour.

The `Background` in the ContentPresenter rule is set to the button's own enabled surface (not `SurfaceMuted`) so the button chrome does not blend into the panel background.

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
| `IconWarning` | Triangle with ! | Validation errors, warnings |
| `IconCheck` | Checkmark in circle | Import success, operation complete |
| `IconRefresh` | Circular arrow | Retry on startup error |

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

### ToolTip

Global style applied in `Components.axaml`. No custom class needed — all `ToolTip.Tip` attributes pick it up automatically.

| Token | Value |
|---|---|
| Background | `AppSurfaceAltBrush` |
| Foreground | `AppTextPrimaryBrush` (cream) |
| BorderBrush | `AppBorderStrongBrush` |
| BorderThickness | 1 |
| CornerRadius | `Radius.Small` |
| Padding | `8,5` |
| FontSize | 12 |

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
    ├── [IsFirstRun]      Border — two-column setup wizard (hero + form)
    ├── [HasStartupError] Border — recovery screen
    └── [HasShell]        Grid  ColumnDefinitions="244,*"
        ├── Col 0: Border (AppSidebarGradientBrush) — sidebar fill (gradient)
        │          Rectangle 1px — divider
        │          Border Padding="14,18" — sidebar content
        │              DockPanel
        │                  Bottom: library card + Settings NavItem + version label
        │                  Fill:   branding pill + tagline + Library/Import NavItems
        └── Col 1: Border (AppSurfaceBrush) — content fill
                   Grid (DataContext=Shell)
                       LibraryView / ImportView / SettingsView (fade-in on :visible)
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
- Buttons (`AccentAction`, `SecondaryAction`, `DestructiveAction`): `Opacity=0.4` on button + ContentPresenter keeps original bg, text muted to `TextMuted`
- Library card in sidebar (`Border.AppPanelMuted`) — `IsEnabled` bound to `!Shell.IsImportInProgress`; dims to 0.45 opacity and prevents Open/New button interaction
- Drop zone in `ImportView` (`Border.DropZone`) — `IsEnabled` bound to `!ImportJobs.IsBusy`; dims to 0.45 opacity, disables the picker buttons, and must not accept drag-over/drop while an import is already running
- OPTIONS panel in `ImportView` (`Border.AppPanelMuted`) — `IsEnabled` bound to `!ImportJobs.IsBusy`; dims both checkboxes (Allow duplicate import, Force conversion to EPUB) via parent propagation
- Only the "Cancel" button remains active

> **Important:** Nav commands must NOT use `CurrentSection is not ShellSection.X` in CanExecute — that would disable the active button and break active-state styling.

---

## 12. Title Bar

Controlled via DWM P/Invoke (`DWMWA_CAPTION_COLOR = 35`, Windows 11 only).  
Colour: `#0D0A07` as COLORREF `0x00070A0D`.  
Set in `MainWindow.axaml.cs → OnOpened`. Catches both `DllNotFoundException` and `EntryPointNotFoundException` for cross-version safety.

---

## 13. App Icon

Generated by `scripts/GenerateLibrovaIcon.py` → `apps/Librova.UI/Assets/librova.ico`.  
Design: warm sepia rounded rectangle background + amber/gold bookmark silhouette with soft glow, highlight strip, spine strip.  
Regenerate: `python scripts\GenerateLibrovaIcon.py apps\Librova.UI\Assets\librova.ico`

---

## 14. Palette Change Checklist

To swap the entire colour palette, update exactly these four locations:

| File | What to change |
|---|---|
| `apps/Librova.UI/Styles/Colors.axaml` | `Color.*` primitive tokens at the top (lines 10–80). All brushes and FluentTheme overrides propagate automatically via `{StaticResource}`. |
| `apps/Librova.UI/Views/MainWindow.axaml.cs` | `TitleBarColor` constant — must match `Color.Background` as COLORREF `0x00BBGGRR`. |
| `apps/Librova.UI/ViewModels/LibraryBrowserViewModel.cs` | 5 `static readonly IBrush` fields (lines ~30–34). Each has a comment naming the token it mirrors. |
| `scripts/GenerateLibrovaIcon.py` | `ACCENT`, `BG`, glow colour constants, then re-run the script to regenerate `librova.ico`. |

**Known Avalonia limitations affecting palette tokens:**

- `BoxShadow` values (e.g., BookCard hover glow `#38F5A623`) and `TextControlSelectionHighlightColor` (`#40F5A623`) cannot reference `StaticResource` because `BoxShadow` is a parsed struct string and `<Color>` text content cannot be a markup extension. Both values are declared as named tokens (`Color.AccentGlow`, `Color.AccentSelection`) in `Colors.axaml` for documentation, but the consuming sites hardcode the literal hex with a comment referencing the token name. Update all four together when changing the palette.
- `AppSidebarGradientBrush` gradient stops also use hardcoded hex values for the same reason — update alongside `Color.Sidebar` / `Color.Background`.

---

## 15. Section Transitions

All three content views (`LibraryView`, `ImportView`, `SettingsView`) have a 150 ms opacity fade-in animation triggered by the `UserControl:visible` pseudo-class. The animation plays each time the section becomes active. No ViewModel changes required — it is self-contained XAML in each view's `UserControl.Styles`.

---

## 16. Sidebar Details

**Gradient** — The sidebar uses `AppSidebarGradientBrush` (a `LinearGradientBrush` from top `#0A0806` to bottom `#1A1409`) to create a subtle depth effect from recessed/shadowed top to warm amber base.

**Version badge** — A small 11 px muted `TextBlock` at the bottom of the sidebar DockPanel, bound to `Shell.ApplicationVersionText`. Only visible when `HasShell=true`.

**Annotation typography** — The book annotation text in the details panel uses `FontStyle="Italic"` to evoke a book excerpt feel.
