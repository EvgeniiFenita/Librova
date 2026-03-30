# Commit Message Guidelines for LibriFlow

This document defines the commit message format for the LibriFlow repository.

The goal is a git history that stays readable months later without opening every diff.

## 1. Message Format

Each commit message must follow this structure:

`<Type>: <Short Summary>`

Examples:

- `Feature: Add EPUB metadata extraction pipeline`
- `Fix: Handle empty author list during duplicate detection`
- `Docs: Update storage layout decision`

## 2. Required Rules

- `<Type>` must be capitalized.
- The summary must use imperative mood.
- The message must not start or end with whitespace.
- Do not use scopes in parentheses.
- Do not use trailing periods in the subject line.
- Keep the subject line concise and high-signal.
- Use plain UTF-8 text without hidden characters.

Correct:

- `Fix: Prevent rollback leak on failed import`
- `Refactor: Split duplicate classification from parser flow`

Incorrect:

- `fix: prevent rollback leak`
- `Feature(parser): Add EPUB support`
- `Added duplicate detection`

## 3. Allowed Types

Use only these commit types:

- `Feature`: new user-visible or architecture-visible functionality
- `Fix`: bug fixes or behavioral corrections
- `Refactor`: internal restructuring without intended behavior change
- `Test`: new tests or meaningful test updates
- `Docs`: documentation changes
- `Build`: build system, dependency, tooling, packaging, or CI changes

## 4. Summary Quality

The summary must describe the actual change, not the activity.

Prefer:

- `Docs: Align code style with LibriFlow architecture`
- `Build: Add vcpkg manifest for native dependencies`
- `Test: Cover probable duplicate confirmation flow`

Avoid:

- `Docs: Update docs`
- `Refactor: Cleanup`
- `Fix: Bug fixes`

## 5. Multi-Line Messages

Use a body when the change needs context.

Recommended body cases:

- architectural decisions;
- non-obvious tradeoffs;
- migrations or breaking changes;
- changes spanning multiple layers.

Body rules:

- Leave one blank line after the subject.
- Write in imperative or explanatory prose.
- Focus on why and impact, not a file-by-file changelog.

## 6. Workflow Rules

- Do not create commits automatically.
- Commit only when explicitly requested by the user.
- If the change is broad or non-trivial, propose a draft message before committing.
- Stage only the intended files for that commit.

## 7. Verification

Before committing:

1. Review the staged diff.
2. Confirm the subject still matches the staged content.
3. Check that the chosen type is accurate.

If the repository has not been initialized with git yet, treat this document as the standard to use once commits begin.
