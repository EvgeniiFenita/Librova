---
name: commit-message
description: "Step-by-step guide for composing and verifying git commits in Librova. Use whenever creating a commit: drafting the message, choosing the type, reviewing staged diff, and applying workflow rules."
---

# Commit Message Guide

## Goal

Produce a high-signal Librova commit message that matches the staged diff and repository workflow rules.

## When to Use

- use this skill only after the user explicitly asked for a commit
- use it to draft or verify the message before running `git commit`
- do **not** create a commit automatically because this skill exists

## Message Format

```text
<Type>: <Short Summary>
```

Examples:

- `Feature: Add EPUB metadata extraction pipeline`
- `Fix: Handle empty author list during duplicate detection`
- `Docs: Update storage layout decision`

## Allowed Types

| Type | When to use |
|---|---|
| `Feature` | New user-visible or architecture-visible functionality |
| `Fix` | Bug fixes or behavioral corrections |
| `Refactor` | Internal restructuring without intended behavior change |
| `Test` | New tests or meaningful test updates |
| `Docs` | Documentation changes |
| `Build` | Build system, dependency, tooling, packaging, or CI changes |

## Subject Rules

- `<Type>` is capitalized
- summary uses imperative mood (`Add`, `Fix`, `Remove`)
- no scope in parentheses
- no trailing period
- no leading or trailing whitespace
- plain UTF-8 only

## Multi-Line Messages

Add a body when the change needs context:

```text
Fix: Prevent rollback leak on failed import

Transaction is now explicitly rolled back before re-throwing the
exception, which prevents the SQLite WAL from growing unboundedly
on repeated import failures.
```

Body rules:

- one blank line after the subject
- explain why and impact
- do not write a file-by-file changelog

## Verification

Before committing:

1. review the staged diff with `git --no-pager diff --cached`
2. confirm the subject still matches the staged content
3. confirm the chosen type is accurate
4. stage only the intended files

## Workflow Rules

- never create a commit automatically
- if the change is broad or non-trivial, draft the message and wait for confirmation before committing
