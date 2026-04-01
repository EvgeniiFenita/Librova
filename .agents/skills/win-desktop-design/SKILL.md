---
name: win-desktop-design
description: >
  A guide for designing modern Windows desktop applications using the
  Avalonia UI + C# stack. Use this skill whenever the user asks to design,
  describe, implement, or improve UI/UX for a Windows application in Avalonia —
  including questions about layouts, navigation, color schemes, dark theme,
  components, styles, XAML markup, typography, icons, animations, and the
  "Windows 11 look". Apply even if the word "design" is not mentioned explicitly —
  any question about the appearance or structure of an Avalonia application is enough.
---

# Modern Windows Desktop App Design with Avalonia UI + C#

## 1. Philosophy and Guidelines

The goal is an application that looks native on Windows 11 while remaining
cross-platform. Use **Microsoft Fluent Design** as the visual language,
adapted to Avalonia's capabilities.

Five Fluent principles:
- **Light** — subtle use of shadows and highlights
- **Depth** — layering (cards, slide-out panels)
- **Motion** — smooth transitions, non-intrusive animations
- **Material** — translucency (Acrylic/Mica effect)
- **Scale** — adaptability to window size and DPI

---

## 2. Stack and Dependencies

```xml
<!-- .csproj — minimum set for a modern look -->
<PackageReference Include="Avalonia" Version="11.*" />
<PackageReference Include="Avalonia.Desktop" Version="11.*" />
<PackageReference Include="Avalonia.Themes.Fluent" Version="11.*" />

<!-- Icons (pick one) -->
<PackageReference Include="FluentIcons.Avalonia" Version="1.*" />
<!-- OR -->
<PackageReference Include="Material.Icons.Avalonia" Version="2.*" />

<!-- MVVM -->
<PackageReference Include="CommunityToolkit.Mvvm" Version="8.*" />
```

Register the theme in `App.axaml`:
```xml
<Application.Styles>
  <FluentTheme />
</Application.Styles>
```

---

## 3. Window Structure (Shell)

### 3.1 NavigationView — Primary Pattern

For most apps use a sidebar navigation via `SplitView`:

```xml
<SplitView IsPaneOpen="{Binding IsPaneOpen}"
           DisplayMode="CompactInline"
           CompactPaneLength="48"
           OpenPaneLength="220">

  <SplitView.Pane>
    <StackPanel>
      <Button Command="{Binding TogglePaneCommand}" Classes="icon-button"
              Margin="4,4,4,8">
        <PathIcon Data="{StaticResource HamburgerIcon}" />
      </Button>

      <ListBox ItemsSource="{Binding NavItems}"
               SelectedItem="{Binding CurrentPage}"
               Classes="nav-list" />
    </StackPanel>
  </SplitView.Pane>

  <SplitView.Content>
    <TransitioningContentControl Content="{Binding CurrentPage}"
                                 PageTransition="{StaticResource SlideTransition}" />
  </SplitView.Content>
</SplitView>
```

### 3.2 Custom TitleBar (Windows 11 style)

```xml
<Window ExtendClientAreaToDecorationsHint="True"
        ExtendClientAreaChromeHints="NoChrome"
        ExtendClientAreaTitleBarHeightHint="-1">

  <Grid RowDefinitions="32,*">
    <Grid Grid.Row="0" IsHitTestVisible="True"
          Background="{DynamicResource AppTitleBarBrush}">
      <TextBlock Text="App Name"
                 VerticalAlignment="Center" Margin="16,0" />
    </Grid>
    <ContentPresenter Grid.Row="1" />
  </Grid>
</Window>
```

---

## 4. Colors and Themes

### 4.1 Auto System Theme Switching

```csharp
// App.axaml.cs
public override void Initialize()
{
    AvaloniaXamlLoader.Load(this);
    RequestedThemeVariant = Application.Current?.ActualThemeVariant
                            == ThemeVariant.Dark
                            ? ThemeVariant.Dark
                            : ThemeVariant.Light;
}
```

### 4.2 Semantic Tokens — Never Hardcode Colors

```xml
<!-- Styles/Colors.axaml -->
<ResourceDictionary>
  <SolidColorBrush x:Key="SurfaceBrush"
                   Color="{DynamicResource SystemChromeMediumLowColor}" />
  <SolidColorBrush x:Key="SurfaceElevatedBrush"
                   Color="{DynamicResource SystemChromeMediumColor}" />
  <SolidColorBrush x:Key="AccentBrush"
                   Color="{DynamicResource SystemAccentColor}" />
  <SolidColorBrush x:Key="TextPrimaryBrush"
                   Color="{DynamicResource SystemBaseHighColor}" />
  <SolidColorBrush x:Key="TextSecondaryBrush"
                   Color="{DynamicResource SystemBaseMediumColor}" />
</ResourceDictionary>
```

### 4.3 Acrylic Background (Windows 11)

```xml
<ExperimentalAcrylicBorder IsHitTestVisible="False" CornerRadius="8">
  <ExperimentalAcrylicBorder.Material>
    <ExperimentalAcrylicMaterial BackgroundSource="Digger"
                                 TintColor="#202020"
                                 TintOpacity="0.6"
                                 MaterialOpacity="0.65" />
  </ExperimentalAcrylicBorder.Material>
</ExperimentalAcrylicBorder>
```

---

## 5. Typography

System font for Windows:
```xml
<Application FontFamily="Segoe UI Variable, Segoe UI, sans-serif">
```

Type scale:

| Role           | Size | FontWeight     |
|----------------|------|----------------|
| Display        | 40   | Light (300)    |
| Title Large    | 28   | SemiBold (600) |
| Title          | 20   | SemiBold (600) |
| Body Large     | 16   | Regular (400)  |
| Body (default) | 14   | Regular (400)  |
| Caption        | 12   | Regular (400)  |

```xml
<!-- Styles/Typography.axaml -->
<Style Selector="TextBlock.title">
  <Setter Property="FontSize" Value="20" />
  <Setter Property="FontWeight" Value="SemiBold" />
</Style>
<Style Selector="TextBlock.caption">
  <Setter Property="FontSize" Value="12" />
  <Setter Property="Foreground" Value="{DynamicResource TextSecondaryBrush}" />
</Style>
```

---

## 6. Grid and Spacing

Base unit = **4px**. All spacing is a multiple of 4:

| Name    | Value | Usage                               |
|---------|-------|-------------------------------------|
| Micro   | 4px   | Between icon and inline text        |
| Small   | 8px   | Inside a component                  |
| Medium  | 12px  | Between items within a group        |
| Base    | 16px  | Standard card padding               |
| Large   | 24px  | Between sections                    |
| XLarge  | 32px  | Between major blocks                |
| Page    | 40px  | Content area inset from window edge |

```xml
<StackPanel Spacing="12">
  <Border Padding="16">
    ...
  </Border>
</StackPanel>
```

---

## 7. Components

### 7.1 Card

```xml
<Style Selector="Border.card">
  <Setter Property="Background" Value="{DynamicResource SurfaceElevatedBrush}" />
  <Setter Property="CornerRadius" Value="8" />
  <Setter Property="Padding" Value="16" />
  <Setter Property="BoxShadow" Value="0 2 8 0 #18000000" />
</Style>
```

### 7.2 Buttons

```xml
<!-- Accent (Primary) -->
<Button Classes="accent" Content="Save" />

<!-- Icon / transparent -->
<Button Classes="icon-button"
        AutomationProperties.Name="Settings">
  <fluent:SymbolIcon Symbol="Settings24Regular" FontSize="20" />
</Button>
```

```xml
<Style Selector="Button.icon-button">
  <Setter Property="Background" Value="Transparent" />
  <Setter Property="Padding" Value="8" />
  <Setter Property="CornerRadius" Value="4" />
</Style>
<Style Selector="Button.icon-button:pointerover /template/ ContentPresenter">
  <Setter Property="Background"
          Value="{DynamicResource SubtleFillColorSecondaryBrush}" />
</Style>
```

### 7.3 Input Field with Validation

```xml
<StackPanel Spacing="4">
  <TextBlock Text="Username" Classes="caption" />
  <TextBox Text="{Binding Username}"
           Watermark="Enter username..." />
  <TextBlock Text="{Binding UsernameError}"
             Classes="caption"
             Foreground="Red"
             IsVisible="{Binding UsernameHasError}" />
</StackPanel>
```

### 7.4 DataGrid

```xml
<DataGrid ItemsSource="{Binding Items}"
          CanUserSortColumns="True"
          CanUserResizeColumns="True"
          GridLinesVisibility="Horizontal"
          SelectionMode="Single">
  <DataGrid.Columns>
    <DataGridTextColumn Header="Name" Binding="{Binding Name}" Width="2*" />
    <DataGridTextColumn Header="Status" Binding="{Binding Status}" Width="*" />
  </DataGrid.Columns>
</DataGrid>
```

---

## 8. Animations and Transitions

```xml
<!-- App.axaml — page transition -->
<CrossFade x:Key="SlideTransition" Duration="0:0:0.15" />
```

Rules:
- Micro-interactions (hover, press): **100–200ms**
- Page transitions: **200–300ms**
- Easing: `CubicEaseOut` on enter, `CubicEaseIn` on exit
- Never animate more than one meaningful element simultaneously

```xml
<!-- Smooth panel fade-in -->
<Border Opacity="{Binding IsVisible, Converter={...}}">
  <Border.Transitions>
    <Transitions>
      <DoubleTransition Property="Opacity"
                        Duration="0:0:0.15"
                        Easing="CubicEaseOut" />
    </Transitions>
  </Border.Transitions>
</Border>
```

---

## 9. Icons

Prefer **FluentIcons.Avalonia** for a Windows-native look:

```xml
<fluent:SymbolIcon Symbol="Home24Regular" FontSize="20" />
<fluent:SymbolIcon Symbol="Add24Filled" FontSize="20"
                   Foreground="{DynamicResource AccentBrush}" />
```

Rules:
- Use a single icon set throughout the entire app
- Sizes: 16px (compact context), 20px (standard), 24px (headings)
- Always set `AutomationProperties.Name` on icon-only buttons

---

## 10. Project Structure

```
MyApp/
├── App.axaml / App.axaml.cs
├── MainWindow.axaml / .cs
├── Assets/
│   └── Icons/
├── Styles/
│   ├── Colors.axaml
│   ├── Typography.axaml
│   └── Components.axaml
├── Views/
│   ├── Shell/
│   │   └── MainShellView.axaml
│   ├── Dashboard/
│   └── Settings/
├── ViewModels/        ← MVVM via CommunityToolkit.Mvvm
├── Models/
└── Services/
```

Register all style dictionaries in `App.axaml`:
```xml
<Application.Resources>
  <ResourceDictionary>
    <ResourceDictionary.MergedDictionaries>
      <ResourceInclude Source="avares://MyApp/Styles/Colors.axaml" />
      <ResourceInclude Source="avares://MyApp/Styles/Typography.axaml" />
      <ResourceInclude Source="avares://MyApp/Styles/Components.axaml" />
    </ResourceDictionary.MergedDictionaries>
  </ResourceDictionary>
</Application.Resources>
```

---

## 11. Accessibility

- Interactive elements — minimum **44×44px**
- `AutomationProperties.Name` on all icon-only buttons
- Text/background contrast ≥ 4.5:1 (WCAG AA)
- Tab navigation works out of the box in the Fluent theme — don't break it

---

## 12. Common Mistakes

| Mistake | Correct approach |
|---------|-----------------|
| Hardcoded `#FFFFFF` / `#000000` | Use `{DynamicResource ...}` tokens |
| Inconsistent `CornerRadius` everywhere | 4 — small elements, 8 — cards, 12 — modals |
| Spacing via `Margin` on every element | Use `StackPanel Spacing="12"` instead |
| Animations > 400ms | Keep under 300ms |
| Mixed icon sets across the app | One icon set for the entire app |
| Overriding hover/pressed states needlessly | The Fluent theme already handles them correctly |
| `Grid` without explicit `RowDefinitions` / `ColumnDefinitions` | Always define them explicitly |
