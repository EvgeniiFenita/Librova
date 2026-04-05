---
name: code-style
description: Code style reference for Librova. Use when writing new C++, C#, Protobuf, or Python code, or when reviewing naming, formatting, or structural conventions.
---

# Code Style Reference

Full authoritative rules: `docs/engineering/CodeStyleGuidelines.md`.  
Read that file when you need full detail. This skill covers only the **project-specific rules that differ from common defaults** — the ones most likely to be missed.

---

## C++ — Non-Obvious Project Rules

### Type Name Prefixes (mandatory)

| Kind | Prefix | Example |
|------|--------|---------|
| Class | `C` | `CImportService` |
| Interface | `I` | `IBookRepository` |
| Struct | `S` | `SBookRecord` |
| Enum | `E` | `EImportResult` |

### Brace Style: Allman (NOT K&R)

```cpp
// Correct
if (isDuplicate)
{
    return result;
}
```

### Member Variables

- Class members: `m_` prefix — `m_repository`, `m_logger`
- Plain DTO / value-object structs: `m_` may be omitted

### Namespaces

```cpp
namespace Librova::Core::Import {

class CImportService
{
};

} // namespace Librova::Core::Import
```

### Unicode and Path

All UTF-8 ↔ wide ↔ `std::filesystem::path` conversions must go through `libs/Unicode/UnicodeConversion.*`.  
Never add local `WideCharToMultiByte` / `MultiByteToWideChar` / `generic_u8string` helpers.

---

## C# Private Fields

`_camelCase` — `_importService`, `_logger` (differs from C++ `m_` convention).

---

## Protobuf — One Non-Obvious Rule

Helper names must reflect actual ownership and return type. Do not use names like `*View` for functions that return owning UTF-8 strings.

---

For everything else (class structure, includes, error handling, testing style, file naming, comments, Python) — read `docs/engineering/CodeStyleGuidelines.md`.
