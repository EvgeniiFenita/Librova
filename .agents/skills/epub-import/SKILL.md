---
name: epub-import
description: Checklist for extending the Librova import pipeline. Use when adding a new source format, modifying import stages, changing duplicate detection, extending conversion handling, or modifying directory/archive import behavior.
---

# Import Pipeline Extension Checklist

## Goal

Change the import pipeline without breaking transactional safety, cancellation semantics, logging, or import-specific docs.

## When to Use

- use this skill when adding a new source format
- use this skill when changing import stages, duplicate detection, conversion handling, or archive / directory behavior
- use `$transport-rpc` alongside this skill if the import change also requires IPC contract updates

> **Architecture overview**: For stage classes, thread model, cancellation contract, and directory layout, see `docs/CodebaseMap.md` §6 Import Pipeline Architecture.

## Pre-Start

- [ ] confirm the task maps to an open backlog item (`python scripts/backlog.py list`)
- [ ] identify which pipeline stage is changing

## Format Support

- [ ] add the parser under `libs/<SliceName>/` with a local `CMakeLists.txt`
- [ ] keep `.hpp` and `.cpp` together
- [ ] declare new vcpkg dependencies explicitly in `vcpkg.json`
- [ ] preserve legacy encodings that still appear in real libraries
- [ ] add unit tests for normal, malformed, and encoding edge cases

## Staging And Commit Safety

- [ ] stage imports before commit — no partial visible success
- [ ] rollback / failure semantics are explicit
- [ ] stale temp state is cleaned on startup
- [ ] strict duplicates are rejected; probable duplicates require explicit user consent
- [ ] integration coverage proves rollback on failure

## Conversion

- [ ] cancellation remains a distinct outcome — never silently fall back to storing the original FB2
- [ ] converter integration stays user-configurable through Settings
- [ ] import behavior still matches the current product rule for plain FB2 vs forced FB2 -> EPUB
- [ ] conversion failure paths are logged through the repository logging facade

## Directory And Archive Import

- [ ] recursive scan follows the same transactional and summary principles as single-file import
- [ ] progress and cancellation are surfaced correctly at the UI level
- [ ] batch operations do not hide silent partial success

## Logging

- [ ] import start, meaningful stage transitions, and import end are logged
- [ ] failure and rollback paths emit actionable logs
- [ ] long-running batch jobs log progress at meaningful intervals

## Close-Out

- [ ] tests are green
- [ ] `docs/Librova-Product.md` updated if user-facing import behavior changed
- [ ] `docs/CodebaseMap.md` updated when import stages, cancellation contract, or storage behavior changed
- [ ] `docs/ReleaseChecklist.md` updated if the UI workflow changed
