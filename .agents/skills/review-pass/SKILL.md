---
name: review-pass
description: Pre-release review and verification checklist for Librova. Use before starting a new major backlog item, after closing a risky vertical slice, or when preparing for MVP release-candidate hardening.
---

# Review Pass Checklist

Use `/review` in the Codex CLI to open a dedicated reviewer that reads the current diff
and reports prioritized findings before you open a pull request.

---

## 1. Runtime Safety

- [ ] Shutdown and disposal paths are deterministic
- [ ] Background work does not outlive destroyed dependencies
- [ ] Partial failures do not leave inconsistent storage or database state
- [ ] Stale temp state is cleaned on startup

---

## 2. IPC and Contract Safety

- [ ] C++ and C# transport method enums are synchronized
- [ ] Request/response ids and parsing rules are covered by tests
- [ ] Timeouts and cancellation semantics are explicit
- [ ] `scripts/ValidateProto.ps1` passes

---

## 3. Import and Conversion Safety

- [ ] Cancellation is a distinct outcome from converter failure
- [ ] No silent fallback to storing the original FB2 on conversion failure
- [ ] Probable duplicates require explicit user consent
- [ ] Rollback semantics are explicit and tested

---

## 4. Unicode Correctness

- [ ] Non-UTF-8 encodings (e.g., Windows-1251) handled in parsers
- [ ] Storage and search paths handle Cyrillic correctly
- [ ] UI source strings use UTF-8

---

## 5. Test Quality

- [ ] Tests cover behavior and user-visible outcomes, not just call counts
- [ ] Strong integration tests are few but protect critical cross-layer paths
- [ ] Fake services model realistic failure outcomes
- [ ] No decorative tests that only restate implementation details
- [ ] `build → test` is sequential (not parallel)

---

## 6. Logging

- [ ] Startup and shutdown are logged with actionable context
- [ ] IPC boundaries emit meaningful log entries
- [ ] Long-running jobs log progress
- [ ] Failure and recovery paths are diagnosable from logs alone
- [ ] Routine polling does not flood logs

---

## 7. Doc Drift Check

Verify these files match implemented reality:

- [ ] `docs/Librova-Product.md`
- [ ] `docs/Librova-Architecture.md`
- [ ] `docs/Librova-Backlog.md`
- [ ] `docs/ManualUiTestScenarios.md` (Russian, UI labels in English as-is)
- [ ] `AGENTS.md`

If any file disagrees with implemented reality, fix it before closing the review pass.

---

## 8. MVP Release Readiness Criteria (from Backlog)

- [ ] `Debug` and `Release` validation both green
- [ ] Manual UI test scenarios walked through successfully
- [ ] Series/genres support implemented and validated
- [ ] No critical regressions in: startup, import, browser, export, delete, settings
- [ ] Logs are actionable enough to diagnose startup and runtime problems
