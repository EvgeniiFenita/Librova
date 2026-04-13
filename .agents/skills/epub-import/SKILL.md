---
name: epub-import
description: Checklist for extending the Librova import pipeline. Use when adding a new source format, modifying import stages, changing duplicate detection, extending conversion handling, or modifying directory/archive import behavior.
---

# Import Pipeline Extension Checklist

Librova import pipeline: source selection → metadata parsing → duplicate detection →
optional conversion (FB2→EPUB) → staging → commit → database write → managed storage placement.

---

## Pre-Start

- [ ] Confirm the task maps to an open backlog item (`python scripts/backlog.py list`); use `$backlog-update` skill if adding a new one
- [ ] Identify which stage of the pipeline is changing

---

## Format Support (new file type)

- [ ] Add parser under `libs/<SliceName>/` with local `CMakeLists.txt`
- [ ] Keep `.hpp` and `.cpp` together in the parser directory
- [ ] Declare any new vcpkg dependencies explicitly in `vcpkg.json`
- [ ] Preserve non-UTF-8 legacy encodings (e.g., Windows-1251 in real FB2 files)
- [ ] Unit tests for the new parser (cover normal, malformed, and encoding edge cases)

---

## Staging and Commit Safety

- [ ] Stage imports before commit — no partial visible success
- [ ] Rollback / failure semantics are explicit
- [ ] Stale temp state is cleaned on startup
- [ ] Strict duplicates are rejected; probable duplicates require explicit user consent
- [ ] Integration test covers rollback on failure

---

## Conversion

- [ ] Cancellation is a **distinct outcome** — never silently fall back to storing the original FB2
- [ ] Converter integration stays user-configurable through the built-in `fb2cng` / `fbc.exe` executable path in `Settings`, and empty path cleanly disables conversion
- [ ] Import behavior matches the current product rule: plain `FB2` storage by default, forced `FB2 -> EPUB` only when the current session has a configured converter and the user explicitly enables it
- [ ] Conversion failure path is logged through the repository logging facade

---

## Directory and Archive Import

- [ ] Recursive scan follows the same transactional and summary principles as single-file import
- [ ] Progress and cancellation are surfaced correctly at the UI level
- [ ] No silent partial success for batch operations

---

## Transport

If the import pipeline change requires new or modified IPC methods, use the `$transport-rpc` skill.

---

## Logging

- [ ] Import start, each stage transition, and import end are logged
- [ ] Failure and rollback paths emit actionable logs
- [ ] Long-running batch jobs log progress at meaningful intervals

---

## Close-Out

- [ ] All tests green (unit + integration)
- [ ] `docs/Librova-Architecture.md` updated if import pipeline structure changed
- [ ] `docs/Librova-Product.md` updated if user-facing import behavior changed
- [ ] `docs/ManualUiTestScenarios.md` updated in Russian if UI import workflow changed
