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

## 3. Release Readiness Criteria

Librova can be treated as MVP-ready when:

- `Debug` and `Release` validation stay green;
- manual UI test scenarios have been walked through successfully;
- series/genres support and Windows `Recycle Bin` delete flow are implemented and validated;
- no critical runtime regressions remain in startup, import, browser, export, delete, or settings flows;
- logs are actionable enough to diagnose startup and runtime problems.

## 4. Out Of Scope For The Current MVP

Not on the active MVP path:

- built-in reading experience;
- metadata editing;
- cloud or sync features;
- multiple libraries;
- plugin system;
- storage sharding and other storage-layout refactors without a proven MVP need;
- large convenience features that do not reduce MVP risk.

## 5. History

Verified checkpoint history is preserved in:

- [Development-History](archive/Development-History.md)
