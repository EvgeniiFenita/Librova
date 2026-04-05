# Librova UI — Local Context

`apps/Librova.UI/` contains the C# / .NET / Avalonia UI layer.  
This layer is responsible for windowing, user interactions, dialogs, ViewModels, and UI-facing services.  
It communicates with `Librova.Core` exclusively via Protobuf over named pipes.

---

## C# Naming

- Public types, methods, properties, enums: `PascalCase`
- Parameters and locals: `camelCase`
- Private fields: `_camelCase` — `_importService`, `_logger`
- Prefer file-scoped namespaces

## ViewModel Rules

ViewModels must **not**:
- Access SQLite or native libraries directly
- Bypass the IPC transport layer
- Hold domain logic

Use async end-to-end for UI-triggered operations that can block. Avoid `async void` except for true event handlers. Keep code-behind thin — UI behavior belongs in ViewModels, not in `.axaml.cs` files.

## UI Design System

Read `docs/UiDesignSystem.md` before any colour, typography, layout, or component change.  
Warm Library palette: base `#0D0A07`, accent amber `#F5A623`, text cream `#F5EDD8`.  
Or use the `$win-desktop-design` skill for full design guidance.

## Build and Test (managed)

```powershell
# Build UI app
dotnet build apps\Librova.UI\Librova.UI.csproj -c Debug

# Build and run managed tests
dotnet test tests\Librova.UI.Tests\Librova.UI.Tests.csproj -c Debug

# Managed only via script
.\Run-Tests.ps1 -SkipNative

# Build everything and launch the app
.\Run-Librova.ps1
.\Run-Librova.ps1 -FirstRun    # first-run setup screen
```

## Manual Test Scenarios

When a UI workflow changes, update `docs/ManualUiTestScenarios.md`.

Format rules:
- Language: **Russian**
- Structure: numbered `## N. <Feature Name>` sections
- Each step on its own numbered line followed by `Ожидаемое поведение:` block
- UI control names appear in **English** exactly as on screen (e.g. `Browse...`, `Continue`, `Import`)
- Append new sections at the end; do not restructure existing ones

Do not load the full file as upfront context — open it only when appending or editing a specific section.
