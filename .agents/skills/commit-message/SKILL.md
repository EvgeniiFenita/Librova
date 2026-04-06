---
name: commit-message
description: "Step-by-step guide for composing and verifying git commits in Librova. Use whenever creating a commit: drafting the message, choosing the type, reviewing staged diff, and applying workflow rules."
---

# Commit Message Guide

## Message Format

```
<Type>: <Short Summary>
```

Examples:
- `Feature: Add EPUB metadata extraction pipeline`
- `Fix: Handle empty author list during duplicate detection`
- `Docs: Update storage layout decision`

---

## Allowed Types

| Type | When to use |
|------|-------------|
| `Feature` | New user-visible or architecture-visible functionality |
| `Fix` | Bug fixes or behavioral corrections |
| `Refactor` | Internal restructuring without intended behavior change |
| `Test` | New tests or meaningful test updates |
| `Docs` | Documentation changes |
| `Build` | Build system, dependency, tooling, packaging, or CI changes |

---

## Rules Checklist

- [ ] `<Type>` is capitalized.
- [ ] Summary uses imperative mood ("Add", "Fix", "Remove" — not "Added", "Fixes").
- [ ] Subject line is concise and high-signal — describes the actual change, not the activity.
- [ ] No scope in parentheses (`Feature(parser):` is wrong).
- [ ] No trailing period in the subject line.
- [ ] No leading or trailing whitespace.
- [ ] Plain UTF-8, no hidden characters.

---

## Multi-Line Message

Add a body when the change needs context (architectural decisions, non-obvious tradeoffs, breaking changes, or changes spanning multiple layers):

```
Fix: Prevent rollback leak on failed import

Transaction is now explicitly rolled back before re-throwing the
exception, which prevents the SQLite WAL from growing unboundedly
on repeated import failures.
```

Body rules:
- One blank line after the subject.
- Imperative or explanatory prose.
- Explain **why** and **impact** — not a file-by-file changelog.

---

## Pre-Commit Verification

- [ ] Review the staged diff (`git --no-pager diff --cached`).
- [ ] Confirm the subject still matches the staged content.
- [ ] Confirm the chosen type is accurate.
- [ ] Stage only the intended files.

---

## Workflow Rules

- **Never create a commit automatically** — only when explicitly requested by the user.
- If the change is broad or non-trivial, **propose a draft message** and wait for confirmation before committing.

---

## Quick Examples

Correct:
- `Fix: Prevent rollback leak on failed import`
- `Refactor: Split duplicate classification from parser flow`
- `Build: Add vcpkg manifest for native dependencies`

Incorrect:
- `fix: prevent rollback leak` — type not capitalized
- `Feature(parser): Add EPUB support` — no scopes
- `Added duplicate detection` — no type, not imperative
- `Docs: Update docs` — non-descriptive summary
