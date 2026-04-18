# Librova UI — Local Context

This file is for `apps/Librova.UI/`-specific context only.

- For global repository rules, workflow, and document ownership, see the root `AGENTS.md`.
- For the canonical style guide, use `$code-style` and `docs/CodeStyleGuidelines.md`.
- For UI shell layout, tokens, and component rules, use `docs/UiDesignSystem.md`.

## Local Scope

`apps/Librova.UI/` contains the C# / .NET / Avalonia UI layer.

- it owns windows, dialogs, ViewModels, and UI-facing services
- it talks to `Librova.Core` only through the protobuf-over-pipes transport
- it should stay free of database and native-domain logic

## ViewModel And UI Boundaries

ViewModels must **not**:

- access SQLite or native libraries directly
- bypass the IPC transport layer
- hold domain logic that belongs in the core

Use async end-to-end for UI-triggered operations that can block. Keep code-behind thin — `.axaml.cs` files are for view wiring, not business logic.

## UI Design References

- `docs/UiDesignSystem.md` is the canonical Librova UI reference.
- `$win-desktop-design` helps with layout and UX reasoning, but it must not override Librova-specific design tokens without updating the design-system doc.

## Managed Quick Loop

```powershell
dotnet build apps\Librova.UI\Librova.UI.csproj -c Debug
dotnet test tests\Librova.UI.Tests\Librova.UI.Tests.csproj -c Debug

# Or use the repo script
.\Run-Tests.ps1 -SkipNative
```

For the full build -> test workflow, including sequential validation rules, follow the root `AGENTS.md`.

## Manual Test Docs

When a UI workflow changes, update `docs/ReleaseChecklist.md` — update the relevant checklist item or add a new one.
