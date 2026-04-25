---
name: cmake
description: "Guide for Librova CMake work: presets, target wiring, Qt module integration, tests, and dependency-safe build changes."
---

# CMake Workflow For Librova

## Goal

Make build-system changes that match Librova's current Windows-first Qt/C++ architecture, preserve the existing target graph, and keep configure/build/test flows predictable.

## Canonical Project References

Use these as the authoritative project examples before inventing a new pattern:

- `CMakeLists.txt`
- `CMakePresets.json`
- `apps/Librova.Qt/CMakeLists.txt`
- `tests/Unit/CMakeLists.txt`
- `tests/Librova.Qt.Tests/CMakeLists.txt`
- `scripts/Run-Tests.ps1`
- `docs/CodebaseMap.md`

This skill is procedural guidance, not a replacement for those files.

## When To Use

- use this skill when editing any `CMakeLists.txt` or `CMakePresets.json`
- use this skill when adding or renaming a native library, Qt app target, or test target
- use this skill when changing Qt target wiring, resources, or the QML module manifest
- use this skill when introducing or updating third-party dependencies through vcpkg
- use this skill when diagnosing configure/build/test failures caused by CMake structure rather than by ordinary C++ code

## Librova Build Defaults

- CMake minimum version is `3.26`
- all native targets build as `C++20` with `CMAKE_CXX_EXTENSIONS OFF`
- repository presets own `binaryDir` and `installDir` under `out/`; do not redirect steady-state artifacts elsewhere
- root configure entry points are the presets in `CMakePresets.json` (`x64-debug`, `x64-release`, `x64-release-static`)
- `vcpkg` is supplied through the preset toolchain file, not by hardcoding package roots in subprojects
- Qt is resolved through `QT_ROOT` / helper scripts; do not hardcode a contributor-local Qt install path in tracked CMake files
- native slices are separate static libraries under `libs/<SliceName>/` with a local `CMakeLists.txt`

## Core Rules

### 1. Stay target-scoped

Prefer `target_*` commands over global project-wide flags.

Good patterns already used in Librova:

```cmake
add_library(LibrovaFoundation STATIC
    UnicodeConversion.cpp
    UnicodeConversion.hpp
)

target_include_directories(LibrovaFoundation PUBLIC
    ${CMAKE_SOURCE_DIR}/libs
)

target_compile_features(LibrovaFoundation PUBLIC cxx_std_20)

target_link_libraries(LibrovaFoundation
    PUBLIC
        spdlog::spdlog_header_only
    PRIVATE
        bcrypt
)
```

Avoid:

- `include_directories(...)`
- `link_libraries(...)`
- `add_definitions(...)`
- ad-hoc global output directory overrides

### 2. Match existing module ownership

When adding a new native slice:

1. create `libs/<SliceName>/CMakeLists.txt`
2. add a `Librova<SliceName>` static library
3. expose `${CMAKE_SOURCE_DIR}/libs` if consumers include by shared root
4. link only the modules the slice actually depends on
5. add `add_subdirectory(libs/<SliceName>)` in root `CMakeLists.txt`
6. update `docs/CodebaseMap.md` if the module is new or renamed

Do not fold unrelated sources into a neighboring target just to avoid one more `add_subdirectory`.

### 3. Keep Windows and Unicode-safe defaults intact

- keep `/utf-8` for MSVC targets that compile project code
- do not remove the static-runtime handling for `VCPKG_TARGET_TRIPLET` values that end with `-static`
- do not move generated files outside `out/`

### 4. Add dependencies through the owning layer

- add new package dependencies in `vcpkg.json`
- declare required optional package features explicitly, as Librova already does for SQLite `fts5`
- add `find_package(... CONFIG REQUIRED)` in the owning module rather than in unrelated parent scopes

## Qt-Specific CMake Rules

Librova's Qt app is already the reference implementation. Follow it instead of introducing a parallel pattern.

- `apps/Librova.Qt/CMakeLists.txt` uses `qt_standard_project_setup(REQUIRES 6.5)`
- the executable target is created with `qt_add_executable(LibrovaQtApp ...)`
- QML files are declared through `qt_add_qml_module(LibrovaQtApp ...)`
- singleton QML files must be marked with `QT_QML_SINGLETON_TYPE TRUE` before `qt_add_qml_module(...)`
- icon and non-QML assets are added through `qt_add_resources(...)`

When adding a new Qt/QML source:

| Change | Update |
|---|---|
| new `.qml` file | `QML_FILES` in `apps/Librova.Qt/CMakeLists.txt` |
| new C++ Qt source/header | `qt_add_executable(LibrovaQtApp ...)` source list |
| new QML singleton | `set_source_files_properties(... QT_QML_SINGLETON_TYPE TRUE)` before module declaration |
| new Win32 resource | `target_sources(... app.rc)` or the existing resource block |

Do not assume a QML file is available at runtime just because it exists on disk; if it is not in `qt_add_qml_module`, it is not part of the packaged module.

## Test Wiring Rules

- keep `build -> test` ordered
- default full validation command: `scripts\Run-Tests.ps1`
- when you need the manual sequence, use:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --config Debug
ctest --test-dir out\build\x64-debug -C Debug --output-on-failure
```

- native tests belong under `tests/Unit`
- Qt tests belong under `tests/Librova.Qt.Tests`

When adding a Qt test, reuse the helpers already defined in `tests/Librova.Qt.Tests/CMakeLists.txt`:

- `add_qt_test`
- `add_qt_test_with_app_sources`
- `add_qt_model_adapter_test`
- `add_qt_controller_test`
- `add_qt_strong_integration_test`

Those helpers already solve the Qt DLL `PATH` setup and common compile options. Do not duplicate that boilerplate per test.

## Preset Rules

- prefer existing presets instead of ad-hoc local configure invocations
- if a new preset is truly needed, keep it under `out/build/<preset>` and `out/install/<preset>`
- preserve the shared hidden `windows-base` pattern for Windows presets
- do not add Linux/macOS presets unless the product direction changes; MVP remains Windows-only

## Common Librova Review Checklist

Before finishing a CMake change, confirm:

1. the target lives in the correct module or test directory
2. all new sources are listed in the owning target
3. Qt/QML files are declared through the QML module, not left loose on disk
4. new dependencies are present in `vcpkg.json`
5. root `add_subdirectory(...)` wiring is updated if a new slice was added
6. docs were updated if module layout or workflow expectations changed
7. `scripts\Run-Tests.ps1` still succeeds

## Anti-Patterns To Avoid

- hardcoding `C:\Qt\...` or another contributor-local path in tracked CMake files
- mixing a new build layout with the repository's `out/` convention
- creating a second Qt app build pattern outside `apps/Librova.Qt/CMakeLists.txt`
- bypassing existing Qt test helper functions with copy-pasted setup
- adding global compiler flags when the change belongs to one target only
