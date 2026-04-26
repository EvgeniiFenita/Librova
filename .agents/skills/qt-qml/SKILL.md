---
name: qt-qml
description: "Guide for Librova Qt/QML work: shell structure, C++/QML boundaries, theme tokens, context properties, and test patterns."
---

# Qt/QML Workflow For Librova

## Goal

Make Qt/QML changes that stay aligned with Librova's one-process architecture, existing shell structure, design system, and Qt test strategy.

## Canonical Project References

Use these as the first project-specific references:

- `docs/UiDesignSystem.md`
- `docs/CodebaseMap.md` §4 and §8
- `apps/Librova.Qt/CMakeLists.txt`
- `apps/Librova.Qt/main.cpp`
- `apps/Librova.Qt/qml/theme/LibrovaTheme.qml`
- `apps/Librova.Qt/qml/theme/LibrovaTypography.qml`
- `apps/Librova.Qt/qml/theme/LibrovaIcons.qml`
- `tests/Librova.Qt.Tests/CMakeLists.txt`

Use `$win-desktop-design` together with this skill when the primary question is appearance, layout, or desktop UX rather than behavior and wiring.

## When To Use

- use this skill when editing `apps/Librova.Qt` C++ shell code
- use this skill when adding or changing QML views, sections, components, or theme files
- use this skill when wiring new controllers, adapters, models, or context properties into QML
- use this skill when adjusting Qt-side tests
- use this skill when changing the app's QML module manifest or runtime resource loading

## Architectural Rules You Must Preserve

- Librova is a one-process Qt/QML shell over the native `ILibraryApplication` facade
- keep domain logic and persistence rules out of QML
- QML owns presentation and local view state
- controllers own shell/startup/preferences/converter-validation workflow
- adapters own backend-facing use-case calls and boundary error handling
- models own list/detail state exposed to QML
- Windows-specific platform behavior belongs in `App/QtWindowsPlatform`, not in arbitrary QML files
- Qt/backend boundary logging is mandatory for important calls and failure paths

If the change becomes a new user-visible cross-layer feature rather than a local Qt edit, pair this skill with `$vertical-slice`.

## Existing Wiring To Follow

These are implemented patterns, not suggestions:

- `QQuickStyle::setStyle(QStringLiteral("Basic"))` happens before `QGuiApplication`
- the QML module URI is `LibrovaQt`
- `Main.qml` is loaded from `qrc:/qt/qml/LibrovaQt/qml/Main.qml`
- app-wide objects are exposed through `engine.rootContext()->setContextProperty(...)`
- current app-wide context properties include `shellController`, `preferences`, `shellState`, `catalogAdapter`, `exportAdapter`, `importAdapter`, `trashAdapter`, `converterValidator`, `firstRunController`, and `windowsPlatform`
- shared theme singletons live in `qml/theme/` and are registered as QML singletons in CMake

When adding a new app-wide context property, keep the name lower-camel-case and wire it in `main.cpp` close to the existing group.

## Directory Ownership

Follow the existing split in `apps/Librova.Qt`:

| Area | Responsibility |
|---|---|
| `App/` | runtime bootstrapping, logging, platform integration, runtime paths, backend dispatcher |
| `Controllers/` | startup, first-run, preferences, shell state, converter validation |
| `Adapters/` | QML-facing wrappers over `ILibraryApplication` |
| `Models/` | Qt models for list/detail/import state |
| `qml/shell/` | shell composition, sidebar, opening/first-run/recovery/fatal-error views |
| `qml/sections/` | top-level Library / Import / Settings sections |
| `qml/library/`, `qml/import/`, `qml/settings/` | section-specific views |
| `qml/components/`, `qml/theme/` | shared reusable controls and theme tokens |

Do not hide a new reusable control inside a section folder just because the first use happens there.

## QML And Visual Rules

- import and reuse shared `LibrovaQt` components before inventing one-off controls
- use `LibrovaTheme`, `LibrovaTypography`, and `LibrovaIcons` instead of hardcoded visual values when a token already exists
- keep toolbar controls with the same role on the same height/chrome family
- popup, dropdown, and flyout surfaces use the elevated warm surface plus accent border pair from `docs/UiDesignSystem.md`
- preserve stable card/grid/detail-panel behavior rather than allowing layout jumpiness
- prefer `LButton`, `LTextInput`, `LCheckBox`, `LNavItem`, `LToast`, and other shared controls where applicable
- keep UI labels in English exactly as implemented in source when documenting or testing them

Use `$win-desktop-design` if you are designing a new surface and need help choosing layout, spacing, typography, or component treatment.

## C++ / QML Boundary Rules

### 1. Do not put business logic in QML

Good:

- QML calls a controller or adapter method
- a model exposes already-prepared display data
- QML keeps only local UI state such as selection, popup visibility, or temporary text

Bad:

- QML reconstructs domain rules
- QML performs filesystem policy decisions
- QML duplicates backend validation or conversion logic

### 2. Use the shared path/unicode helpers

When Qt strings cross into native filesystem code:

- route UTF-8 to `std::filesystem::path` through `Librova::Unicode::PathFromUtf8(...)`
- prefer existing helpers such as `Adapters/QtStringConversions.hpp`
- never construct `std::filesystem::path` from UTF-8 `std::string` directly on Windows

### 3. Keep success and failure paths explicit

- adapters should surface backend errors through explicit signals / error callbacks / model state
- do not continue with success-shaped UI updates after backend exceptions
- use toasts or the owning view's explicit error presentation pattern for non-blocking failures

## Common Task Patterns

### Add a new QML file

1. place the file in the correct `qml/` subfolder
2. add it to `qt_add_qml_module(... QML_FILES ...)` in `apps/Librova.Qt/CMakeLists.txt`
3. if it is reusable, prefer `qml/components/`
4. if it changes visible workflow or component rules, update `docs/UiDesignSystem.md` or `docs/ReleaseChecklist.md` as appropriate

### Add a new QML singleton

1. put it under `qml/theme/` only if it is truly app-wide token/style data
2. mark it with `QT_QML_SINGLETON_TYPE TRUE` before `qt_add_qml_module(...)`
3. keep the singleton API small and token-oriented

### Add a new backend-driven UI capability

1. decide whether it belongs in a controller, adapter, or model
2. extend native application/domain code first if the Qt layer needs a new contract
3. expose the result through the Qt owner type instead of pushing domain work into QML
4. wire QML bindings and signal handling
5. add the right test layer in `tests/Librova.Qt.Tests`

## Test Rules

- prefer controller/model/adapter tests and shell composition tests over UI automation
- use the helper functions already defined in `tests/Librova.Qt.Tests/CMakeLists.txt`
- add a strong Qt integration test only when the change genuinely crosses `QML/Qt adapter -> C++ facade` and lower-level coverage is not enough
- if a QML-visible file is added, make sure the parity/resource tests still cover it through the module manifest

Typical Qt test categories already present:

- controller-only tests
- app/runtime-path tests
- model + adapter tests
- strong Qt integration tests
- QML parity tests

## Per-Corner Radius on Rectangle

Qt 6.7+ supports individual corner radii directly on `Rectangle` — use these instead of hacks (clip tricks, stacked rectangles):

```qml
Rectangle {
    topLeftRadius:     LibrovaTheme.radiusMedium
    bottomLeftRadius:  LibrovaTheme.radiusMedium
    topRightRadius:    LibrovaTheme.radiusMedium
    bottomRightRadius: LibrovaTheme.radiusMedium
}
```

Use `topRightRadius` / `bottomRightRadius` (without `topLeftRadius` / `bottomLeftRadius`) when a control is the **right segment** of a compound input (e.g. a clear/action button attached to the right edge of a text field). The left side stays square; only the right side is rounded — matching the outer container's rounding. See `LibraryToolbar.qml` and the converter clear button in `SettingsView.qml` for live examples.

## Common Mistakes To Avoid

- hardcoding colours, spacing, or control sizes when a theme token already exists
- adding a QML file on disk but forgetting to register it in `qt_add_qml_module`
- putting domain logic into `onClicked`, `Component.onCompleted`, or ad-hoc JavaScript helpers
- bypassing shared Qt models/adapters with direct backend calls from random UI code
- introducing Windows-specific UI behavior directly in QML instead of through `QtWindowsPlatform`
- converting `QString` to filesystem paths without `PathFromUtf8(...)`

## Close-Out Checklist

Before finishing a Qt/QML change, confirm:

1. the change lives in the correct Qt layer
2. new QML/C++ files are registered in `apps/Librova.Qt/CMakeLists.txt`
3. shared controls/theme tokens were reused where applicable
4. errors are surfaced explicitly and success paths are not run after backend failure
5. the right Qt test layer was updated
6. `docs/UiDesignSystem.md`, `docs/ReleaseChecklist.md`, or `docs/CodebaseMap.md` were updated if ownership says they should be
7. `scripts\Run-Tests.ps1` still passes
