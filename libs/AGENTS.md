# Librova Native Core — Local Context

Libraries under `libs/` contain the C++20 domain and infrastructure slices of `Librova.Core`.  
Each slice is a static library with its own `CMakeLists.txt`. Keep `.hpp` and `.cpp` together.

---

## C++ Naming (project-specific — differs from common defaults)

| Kind | Rule | Example |
|------|------|---------|
| Class | `C` prefix | `CImportService` |
| Interface | `I` prefix | `IBookRepository` |
| Struct | `S` prefix | `SBookRecord` |
| Enum | `E` prefix | `EImportResult` |
| Method | `PascalCase` | `ParseMetadata()` |
| Parameter / local | `camelCase` | `bookId` |
| Member variable | `m_` prefix | `m_repository` |

Plain DTO / value-object structs may omit `m_`.

## Brace Style: Allman (NOT K&R)

```cpp
if (condition)
{
    doSomething();
}
```

## Namespaces

```cpp
namespace Librova::Core::Import {

class CImportService
{
};

} // namespace Librova::Core::Import
```

## Unicode and Path Conversions

All UTF-8 ↔ wide-string ↔ `std::filesystem::path` conversions must go through `libs/Unicode/UnicodeConversion.*`.  
Never add local `WideCharToMultiByte`, `MultiByteToWideChar`, `generic_u8string`, or duplicate path-conversion helpers.

## Logging

Use `spdlog` through the repository logging facade. Never write to `std::cout` or `std::cerr` from reusable library code. Log actionable context at startup, shutdown, IPC boundaries, long-running operations, and failure paths.

## Build and Test (native)

```powershell
# Configure + build
cmake --preset x64-debug
cmake --build --preset x64-debug --config Debug

# Run native tests
ctest --test-dir out\build\x64-debug -C Debug --output-on-failure

# Native only via script
.\Run-Tests.ps1 -SkipManaged
```

Run `scripts\ValidateProto.ps1` after any change under `proto/`.
