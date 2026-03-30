# Librova Roadmap

## 1. Current Stage

Librova is in late MVP stabilization.

Major functional MVP flows are already implemented end-to-end:

- import;
- browser and details;
- export;
- delete-to-trash;
- first-run setup;
- settings for next launch and converter configuration.

## 2. Remaining MVP Work

The remaining roadmap is primarily stabilization work:

- broad review passes on runtime behavior and crash safety;
- final cleanup of logging and error surfaces;
- manual UI verification against the maintained scenario checklist;
- final release-candidate hardening;
- documentation cleanup when implemented reality changes.

## 3. Release Readiness Criteria

Librova can be treated as MVP-ready when:

- `Debug` and `Release` validation stay green;
- manual UI test scenarios have been walked through successfully;
- no critical runtime regressions remain in startup, import, browser, export, trash, or settings flows;
- logs are actionable enough to diagnose startup and runtime problems.

## 4. Out Of Scope For The Current MVP

Not on the active MVP path:

- built-in reading experience;
- metadata editing;
- cloud or sync features;
- multiple libraries;
- plugin system;
- large convenience features that do not reduce MVP risk.

## 5. History

Verified checkpoint history is preserved in:

- [Development-History](C:\Users\evgen\Desktop\Librova\docs\archive\Development-History.md)
