---
name: review-pass
description: Pre-release review and verification checklist for Librova. Use before starting a new major backlog item, after closing a risky vertical slice, or when preparing for MVP release-candidate hardening.
---

# Review Pass Checklist

## Goal

Run a high-signal hardening pass over risky changes before review, release, or a new major implementation wave.

## When to Use

- use this skill after high-risk changes touching transport, storage, cancellation, rollback, startup, shutdown, or other failure-prone boundaries
- use this skill before a release-candidate pass
- use this skill when you want to check for documentation drift before handing work off
- do **not** use this as a replacement for the implementation checklist of a feature; use it after the implementation work exists

Use `/review` in the Codex CLI to open a dedicated reviewer for the current diff when code review is the goal.

## 1. Runtime Safety

- [ ] shutdown and disposal paths are deterministic
- [ ] background work does not outlive destroyed dependencies
- [ ] partial failures do not leave inconsistent storage or database state
- [ ] stale temp state is cleaned on startup

## 2. IPC And Contract Safety

- [ ] C++ and C# transport method enums are synchronized
- [ ] request/response ids and parsing rules are covered by tests
- [ ] timeouts and cancellation semantics are explicit
- [ ] `scripts/ValidateProto.ps1` passes when transport changed

## 3. Import And Conversion Safety

- [ ] cancellation is distinct from converter failure
- [ ] no silent fallback to storing the original FB2 on conversion failure
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
- [ ] build -> test execution stayed sequential

## 6. Logging

- [ ] startup and shutdown are logged with actionable context
- [ ] IPC boundaries emit meaningful log entries
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
- [ ] required manual UI scenarios were walked through
- [ ] no critical regressions remain in startup, import, browser, export, delete, or settings
- [ ] logs are actionable enough to diagnose startup and runtime problems
- [ ] remaining open backlog items are intentionally scheduled, not accidental drift
