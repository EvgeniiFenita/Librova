# Librova Review Checklist

Use this checklist before starting a new major bucket or after closing a risky vertical slice.

## 1. Runtime Safety

- shutdown and disposal paths are deterministic
- background work does not outlive destroyed dependencies
- partial failures do not leave inconsistent storage or database state

## 2. IPC And Contract Safety

- C++ and C# transport enums are synchronized
- request/response ids and parsing rules are covered by tests
- timeouts and cancellation semantics are explicit

## 3. Test Quality

- tests cover behavior, not just call counts
- strong integration tests are few but valuable
- fake services model realistic outcomes
- command-path tests do not bypass important validation

## 4. Logging

- important boundaries log actionable context
- routine polling does not flood logs
- startup and shutdown failures are diagnosable from logs

## 5. Docs Drift

Check together:

- `docs/Librova-Architecture-Master.md`
- `docs/ProjectDocumentation.md`
- `docs/ImplementationProgress.md`
- `CodexSessionInstructions.md`

If one of them disagrees with implemented reality, fix it in the same task.
