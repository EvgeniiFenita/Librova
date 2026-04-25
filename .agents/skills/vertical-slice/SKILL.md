---
name: vertical-slice
description: End-to-end implementation checklist for any new Librova feature. Use when starting a read-side query, mutation, or UI shell workflow. Do NOT use for pure stabilization, doc-only, or build-only tasks.
---

# Vertical Slice Playbook

## Goal

Deliver a feature or workflow end-to-end across every touched layer, with tests and documentation updated in the same task.

## When to Use

- use this skill for user-visible features or workflow changes that cross multiple layers
- use this skill for read-side or mutation flows that touch domain, Qt adapters/controllers, and UI together
- use `$import-pipeline` as the domain-specific companion when the slice is mainly about import behavior
- use `$review-pass` instead for stabilization or hardening passes without new feature work

## 0. Pre-Start Gate

Before writing code:

1. run `python scripts/backlog.py list` and confirm the task maps to an open backlog item
2. if no backlog item matches, stop and confirm with the user whether to proceed
3. identify the slice category: read-side query / mutation-use-case / UI-shell-workflow
4. consult `docs/CodebaseMap.md` §9 Task Navigation for the entry-point checklist for your slice type, and §3-§4 to locate the relevant modules

## 1. Read-Side Query Slice

Use for list, details, search, filters, and similar read-only flows.

- [ ] add or extend the native application facade
- [ ] keep domain and repository boundaries clean unless a new contract is genuinely required
- [ ] add or extend the Qt adapter/controller/model surface
- [ ] add or extend the QML binding
- [ ] add unit tests for local logic
- [ ] add an integration test for the touched Qt/backend boundary
- [ ] update docs according to `AGENTS.md` document-maintenance policy

## 2. Mutation / Use-Case Slice

Use for import, delete, export, conversion, and any state-changing flow.

Apply the read-side checklist plus:

- [ ] staging before commit — no partial visible success
- [ ] rollback / failure semantics are explicit and tested
- [ ] cancellation is a distinct outcome, not a silent fallback
- [ ] cleanup of stale temp state is handled
- [ ] logging covers long-running work and failure paths
- [ ] `docs/Librova-Product.md` updated if user-facing behavior changed
- [ ] `docs/ReleaseChecklist.md` updated if the UI workflow changed

## 3. UI Shell Workflow Slice

Use for a new section, dialog, settings panel, or first-run flow.

- [ ] dialog abstraction behind an interface where platform UI would otherwise be hard to test
- [ ] ViewModel logic covered by unit tests
- [ ] shell composition test for the new workflow
- [ ] `docs/ReleaseChecklist.md` updated if the UI workflow changed
- [ ] UI labels in English exactly as they appear in the source

## 4. Close-Out

Before marking the task done:

- [ ] build and test validation complete — follow the checklist in `$review-pass` §8
- [ ] docs updated per `AGENTS.md` document-maintenance policy
- [ ] backlog item closed using `$backlog-update`
- [ ] no decorative tests added
- [ ] no commit made unless explicitly requested
