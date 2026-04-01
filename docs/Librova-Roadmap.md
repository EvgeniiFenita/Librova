# Librova Roadmap

## 1. Current Stage

Librova has a working MVP baseline, but active MVP work is still continuing.

Already implemented end-to-end:

- single-file, multi-file, directory, and archive import;
- browser and details;
- export;
- first-run setup;
- settings for converter configuration;
- logging, host lifecycle, and manual validation baseline.

## 2. Remaining MVP Work

The remaining active MVP scope is:

- stronger support for series and genres across parsing, storage, details, and browser filtering;
- replacing the current managed-library `Trash` implementation with Windows `Recycle Bin` integration;
- broad review passes on runtime behavior and crash safety;
- final cleanup of logging and error surfaces;
- manual UI verification against the maintained scenario checklist;
- final release-candidate hardening;
- documentation cleanup when implemented reality changes.

Library browsing polish that is explicitly deferred until a later library UX iteration:

- broader library UX polish beyond the MVP-safe series/genres work;
- storage sharding or per-book storage-layout redesign.

## 3. MVP Blockers From Review Pass

The release-blocking runtime issues confirmed by the review pass have now been closed and re-verified:

- SQLite database opening is Unicode-safe on Windows.
- Host shutdown no longer relies on watchdog-driven `ExitProcess`.
- External converter child processes are bound to a Windows `Job Object`, and cancellation cleans up produced output files.

## 4. Iterative Stabilization Plan

Work should move in small checkpoints, each ending with updated tests and docs when implemented reality changes.

### Iteration 1 — SQLite Unicode Path Safety

Status: completed and verified.

- switch native SQLite open path handling to explicit UTF-8 conversion;
- add verification that a database under a Unicode or Cyrillic path opens and migrates successfully.

### Iteration 2 — Graceful Host Shutdown

Status: completed and verified.

- replace watchdog-driven `ExitProcess` termination with a graceful shutdown signal;
- ensure destructor-based cleanup, SQLite close, and log flushing remain on the normal process-exit path;
- add verification for parent-process termination behavior.

### Iteration 3 — Converter Lifetime Safety

Status: completed and verified.

- bind external converter processes to a Windows `Job Object` with kill-on-close semantics;
- keep cancellation as a distinct outcome and clean up converter outputs after cancellation;
- add verification that cancelled or aborted sessions do not leave converter processes or partial outputs behind.

### Iteration 4 — SQLite Runtime Hardening

Status: completed and verified.

- add `busy_timeout` protection for concurrent read/write access patterns;
- verify search and import behavior under overlapping database access.

### Iteration 5 — FB2 Parsing Hardening

- replace locale-dependent numeric parsing in FB2 metadata extraction;
- keep Windows-1251 support while tightening parser behavior through targeted tests.

### Iteration 6 — IPC Contract Guardrails

- add explicit validation that C++ and C# pipe method enums stay synchronized;
- treat transport drift checks as part of routine validation for future RPC work.

### Iteration 7 — Logging And Failure Diagnostics

- stop silently swallowing rollback failures in trash-related recovery paths;
- make export overwrite behavior explicit in code, logs, and, if needed, the user-facing contract.

### Iteration 8 — Code Health Cleanup

- remove duplicated path-safety and UTF-8 helper logic where it creates maintenance risk;
- harden ZIP archive wrappers against accidental copy semantics;
- clean up smaller review-pass issues that are real but not release-blocking.

## 5. Release Readiness Criteria

Librova can be treated as MVP-ready when:

- `Debug` and `Release` validation stay green;
- the confirmed MVP blockers from the review pass are closed and re-verified;
- manual UI test scenarios have been walked through successfully;
- series/genres support and Windows `Recycle Bin` delete flow are implemented and validated;
- no critical runtime regressions remain in startup, import, browser, export, delete, or settings flows;
- logs are actionable enough to diagnose startup and runtime problems.

## 6. Out Of Scope For The Current MVP

Not on the active MVP path:

- built-in reading experience;
- metadata editing;
- cloud or sync features;
- multiple libraries;
- plugin system;
- storage sharding and other storage-layout refactors without a proven MVP need;
- large convenience features that do not reduce MVP risk.

## 7. History

Verified checkpoint history is preserved in:

- [Development-History](archive/Development-History.md)
