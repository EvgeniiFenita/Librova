---
name: code-style
description: Code style reference for Librova. Use when writing new C++, QML, or Python code, or when reviewing naming, formatting, or structural conventions.
---

# Code Style Reference

## Canonical Source

Full authoritative rules: `docs/CodeStyleGuidelines.md`.

This skill only highlights the rules most likely to be missed.

## Goal

Resolve Librova-specific style questions quickly and defer to the full canonical guide when the short reminders are not enough.

## When to Use

- use this skill when naming, formatting, or structure is unclear
- use this skill during review when a rule might differ from common ecosystem defaults
- do **not** use this as a replacement for `docs/CodeStyleGuidelines.md`; that file remains canonical

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

### Qt / QML boundaries

- keep QML focused on presentation and view state
- keep business rules in native application/domain layers or QML-facing Qt adapters and controllers
- reuse shared QML controls and theme tokens instead of hardcoded per-view styling

### Unicode and path handling

- route UTF-8 / wide / path conversions through `libs/Foundation/UnicodeConversion.*`
- never add local `WideCharToMultiByte`, `MultiByteToWideChar`, or `generic_u8string` helpers

## If You Need More Than This

Read `docs/CodeStyleGuidelines.md` before making style-sensitive edits in unfamiliar code.

## What This Skill Does Not Cover

The following categories are intentionally omitted here —
consult `docs/CodeStyleGuidelines.md` for authoritative rules:

- `#include` ordering and grouping
- namespace declaration style
- error handling and exception conventions
- file and directory naming
