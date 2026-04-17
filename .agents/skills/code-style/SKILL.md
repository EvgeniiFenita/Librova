---
name: code-style
description: Code style reference for Librova. Use when writing new C++, C#, Protobuf, or Python code, or when reviewing naming, formatting, or structural conventions.
---

# Code Style Reference

## Goal

Resolve Librova-specific style questions quickly and defer to the full canonical guide when the short reminders are not enough.

## When to Use

- use this skill when naming, formatting, or structure is unclear
- use this skill during review when a rule might differ from common ecosystem defaults
- do **not** use this as a replacement for `docs/engineering/CodeStyleGuidelines.md`; that file remains canonical

## Canonical Source

Full authoritative rules: `docs/engineering/CodeStyleGuidelines.md`.

This skill only highlights the rules most likely to be missed.

## Most-Missed Rules

### C++ type prefixes

| Kind | Prefix | Example |
|---|---|---|
| Class | `C` | `CImportService` |
| Interface | `I` | `IBookRepository` |
| Struct | `S` | `SBookRecord` |
| Enum | `E` | `EImportResult` |

### C++ formatting

- Allman braces, not K&R
- member variables use `m_`
- nested namespace syntax is preferred

Correct:

```cpp
if (isDuplicate)
{
    return result;
}
```

Incorrect:

```cpp
if (isDuplicate) {
    return result;
}
```

### C# private fields

- private fields use `_camelCase`
- public types, methods, properties, and enums use `PascalCase`

### Unicode and path handling

- route UTF-8 / wide / path conversions through `libs/Unicode/UnicodeConversion.*`
- never add local `WideCharToMultiByte`, `MultiByteToWideChar`, or `generic_u8string` helpers

### Proto naming

- message and enum names: `PascalCase`
- field names: `snake_case`
- helper names must reflect actual ownership and return type; do not use names like `*View` for owning UTF-8 strings

## If You Need More Than This

Read `docs/engineering/CodeStyleGuidelines.md` before making style-sensitive edits in unfamiliar code.
