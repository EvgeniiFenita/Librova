# Librova Test Strategy

This document defines what kinds of tests the project should add and when.

## 1. Core Principles

- Prefer the cheapest test that still protects the real risk.
- Add stronger integration only where unit coverage would miss cross-layer failures.
- Avoid decorative tests that only restate implementation details.
- Keep verification sequential: build first, then tests.

## 2. Test Layers

### Unit Tests

Use for:

- local business rules
- mappers
- validation logic
- DTO conversion
- command enablement
- pagination and state transitions

Unit tests should dominate by count.

### Integration Tests

Use for:

- real SQLite behavior
- filesystem staging and rollback
- native host composition
- named-pipe request/response
- C# to native host flows through real IPC

Integration tests should exist at critical boundaries, not everywhere.

### Strong Host-Backed Tests

Use sparingly for the most valuable end-to-end user flows.

Good examples:

- import through the real native host followed by browser refresh
- browser query against the real native host and SQLite
- export through the real native host and managed library

These tests are expensive but valuable because they catch cross-layer drift.

## 3. When A New Test Is Required

Add a new test when:

- a new user-facing behavior is introduced
- a new IPC method is introduced
- rollback or partial-failure semantics are introduced
- a review found a bug class not covered by current tests
- C++ and C# contracts can drift independently
- a fix changes Unicode, encoding, path, or filesystem-boundary behavior
- a fix changes startup, shutdown, or command-line control flow

## 4. Fake-Test Smell

Be suspicious when:

- a test bypasses validation or command entry points
- a fake implementation never models the real failure mode
- a test checks that a method was called but not the user-visible outcome
- a ViewModel test uses invalid paths or impossible state unless that is the scenario under test

Specific regression classes that should remain covered once fixed:

- Unicode or Cyrillic filesystem paths
- legacy text encodings such as `windows-1251`
- rollback and cancellation cleanup
- cross-language enum or transport drift
- search-index and database side effects, not only returned DTOs
- CLI parsing for non-runtime flags such as `--help` and `--version`

## 5. UI Test Policy

- Prefer ViewModel and shell composition tests over UI automation.
- Add UI automation only when visual/control behavior itself becomes the risk.
- Keep dialogs behind abstractions so UI behavior remains testable without the real platform.

## 6. Verification Checklist

For a meaningful vertical slice, prefer this order:

1. unit tests for local logic
2. integration tests for touched boundaries
3. strong host-backed test if the feature crosses `C# -> pipe -> C++`

Do not add a strong integration test if existing lower-level tests already fully protect the change.

When a review-pass bug is fixed, add the narrowest regression test that proves the original failure mode stays closed.
