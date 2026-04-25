---
name: review-pass
description: Pre-release review and verification checklist for Librova. Use before starting a new major backlog item, after closing a risky vertical slice, or when preparing for MVP release-candidate hardening.
---

# Review Pass Checklist

## Goal

Run a high-signal hardening pass over risky changes before review, release, or a new major implementation wave.

## When to Use

- use this skill after high-risk changes touching Qt/backend boundaries, storage, cancellation, rollback, startup, shutdown, or other failure-prone boundaries
- use this skill before a release-candidate pass
- use this skill when you want to check for documentation drift before handing work off
- do **not** use this as a replacement for the implementation checklist of a feature; use it after the implementation work exists
- for a combined flow: run `$vertical-slice` first for implementation, then `$review-pass` for hardening — do not reverse the order

Use `/review` in the Codex CLI to open a dedicated reviewer for the current diff when code review is the goal.

## 1. Runtime Safety

- [ ] shutdown and disposal paths are deterministic
- [ ] background work does not outlive destroyed dependencies
- [ ] partial failures do not leave inconsistent storage or database state
- [ ] stale temp state is cleaned on startup

## 2. Qt / Backend Contract Safety

- [ ] Qt adapters and native application facade contracts are synchronized
- [ ] request/response mapping rules are covered by tests
- [ ] timeouts and cancellation semantics are explicit

## 3. Import And Conversion Safety

- [ ] cancellation is distinct from converter failure
- [ ] no silent fallback to storing the original FB2 on conversion failure
- [ ] `import_covers=false` skips cover extraction throughout the entire pipeline, not only at the final storage step
- [ ] probable duplicates require explicit user consent
- [ ] rollback semantics are explicit and tested

## 4. Unicode Correctness

- [ ] non-UTF-8 encodings (for example Windows-1251) are handled where required
- [ ] storage and search paths handle Cyrillic correctly
- [ ] UI source strings remain valid UTF-8 / Unicode text

## 5. Test Quality

- [ ] tests cover behavior and user-visible outcomes, not just call counts
- [ ] strong integration tests are few but protect critical cross-layer paths
- [ ] fake services model realistic failure outcomes
- [ ] no decorative tests that only restate implementation details
- [ ] build finished before any dependent test suite started; any parallel test execution happened only after the required build completed

## 6. Logging

- [ ] startup and shutdown are logged with actionable context
- [ ] Qt/backend boundaries emit meaningful log entries
- [ ] long-running jobs log progress
- [ ] failure and recovery paths are diagnosable from logs alone
- [ ] routine polling does not flood logs

## 7. Doc Drift Check

Verify these match implemented reality:

- [ ] `docs/Librova-Product.md`
- [ ] `docs/CodebaseMap.md`
- [ ] `python scripts/backlog.py list` / `python scripts/backlog.py show <id>`
- [ ] `docs/ReleaseChecklist.md` when a UI scenario changed
- [ ] `AGENTS.md`

If any of these drifted, fix them immediately using the relevant doc owner from `AGENTS.md` instead of leaving follow-up cleanup behind.

## 8. Release Readiness

- [ ] `Debug` and `Release` validation are complete when code changed
- [ ] required manual UI scenarios in `docs/ReleaseChecklist.md` were walked through
- [ ] no critical regressions remain in startup, import, browser, export, delete, or settings
- [ ] logs are actionable enough to diagnose startup and runtime problems
- [ ] remaining open backlog items are intentionally scheduled, not accidental drift

## 9. Filesystem / Path Safety

- [ ] all `std::filesystem::path` constructions from UTF-8 strings use `PathFromUtf8()`, never `path(std::string)` or `path(const char*)`
- [ ] no absolute path leaks into the database or preferences unless intentionally persisted
- [ ] library root and runtime workspace are not mixed without explicit intent
- [ ] managed library is portable: no hard-coded drive letters or machine-specific paths stored durably
- [ ] `ManagedPathSafety` escape checks cover archive and export destination paths

## 10. SQLite / Schema Safety

- [ ] schema version policy enforced: v0→2 and v1→2 are the only approved upgrade paths; any other version produces an incompatibility error, not a silent migration
- [ ] `ON DELETE CASCADE` on `book_collections` removes only membership rows, not the books themselves
- [ ] FTS5 queries are safe against punctuation and special-character input
- [ ] SQLite transaction boundaries cover all multi-step writes

## 11. Collections Correctness

- [ ] delete collection removes only membership rows; books must survive
- [ ] add/remove book from collection is idempotent and leaves no orphan rows
- [ ] sidebar and browser correctly reflect membership changes without stale state
- [ ] application facade and Qt adapter collection operations are covered: `ListCollections`, `CreateCollection`, `DeleteCollection`, `AddBookToCollection`, `RemoveBookFromCollection`

## 12. Project-Specific Invariants

- [ ] `zip_t*` ZIP access stays single-threaded; no ZIP handle shared across worker threads
- [ ] cancellation of conversion is **not** classified as ordinary converter failure
- [ ] `IBookRepository::ReserveIds()` called on main thread before worker dispatch; workers never open their own SQLite transactions for ID allocation
- [ ] all `std::future` in Phase 2 of `CZipImportCoordinator` resolved via `get()` even after cancellation — abandoning futures is UB
- [ ] `inFlight` semaphore declared before `thread_pool` in `CZipImportCoordinator`; reversed order causes dangling reference on destruction
- [ ] export destination written via temporary sibling, not in-place
