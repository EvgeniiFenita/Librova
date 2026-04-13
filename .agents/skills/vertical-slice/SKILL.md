---
name: vertical-slice
description: End-to-end implementation checklist for any new Librova feature. Use when starting a read-side query, mutation, transport contract change, or UI shell workflow. Do NOT use for pure stabilization, doc-only, or build-only tasks.
---

# Vertical Slice Playbook

## 0. Pre-Start Gate

Before writing any code:

1. Run `python scripts/backlog.py list` and confirm the task maps to an open backlog item.
2. If no backlog item matches, **stop** and confirm with the user whether to proceed.
3. Identify the slice category: read-side query / mutation-use-case / transport-contract / UI-shell-workflow.

---

## 1. Read-Side Query Slice

Use for: list, details, search, filters, and similar read-only flows.

Checklist:

- [ ] Add or extend native application facade
- [ ] Keep domain and repository boundaries unchanged (unless a new contract is needed)
- [ ] Add or extend protobuf request/response messages in `proto/`
- [ ] Extend proto mapping on native side
- [ ] Extend protobuf service adapter (native)
- [ ] Extend pipe transport method registration and request dispatch (native)
- [ ] Run `scripts/ValidateProto.ps1`
- [ ] Add or extend C# `PipeProtocol` method enum
- [ ] Add or extend C# client method
- [ ] Add or extend C# service and mapper
- [ ] Add or extend C# ViewModel / UI binding
- [ ] Unit tests for local logic
- [ ] Integration test for the IPC boundary
- [ ] Update `docs/Librova-Architecture.md` if any structural decision changed

---

## 2. Mutation / Use-Case Slice

Use for: import, delete, export, conversion, any state-changing flow.

Additional requirements on top of the read-side checklist:

- [ ] Staging before commit — no partial visible success
- [ ] Rollback / failure semantics are explicit and tested
- [ ] Cancellation is a distinct outcome (not silent fallback)
- [ ] Cleanup of stale temp state is handled
- [ ] Logging covers the long-running job path and failure path
- [ ] Update `docs/Librova-Product.md` if the user-facing behavior changed
- [ ] Update `docs/ManualUiTestScenarios.md` (in Russian) if UI workflow changed

---

## 3. Transport Contract Change Slice

Use `$transport-rpc` skill instead — it has the full IPC-specific checklist.

---

## 4. UI Shell Workflow Slice

Use for: new section, dialog, settings panel, first-run flow.

Checklist:

- [ ] Dialog abstraction behind an interface (keeps it testable without the real platform)
- [ ] ViewModel logic covered by unit tests
- [ ] Shell composition test for the new workflow
- [ ] Manual UI scenario added to `docs/ManualUiTestScenarios.md` (in Russian)
- [ ] UI labels in English exactly as they appear in the source

---

## 5. Close-Out

Before marking the task done:

- [ ] `Debug` and `Release` build both pass
- [ ] All new tests are green
- [ ] `docs/` updated for any changed product / architecture / backlog reality
- [ ] Backlog item closed using `$backlog-update` skill (`python scripts/backlog.py close <id> --note="…"`; moved to `docs/backlog-archive.yaml`)
- [ ] No decorative tests added (see `docs/engineering/TestStrategy.md`)
- [ ] No commits made unless explicitly requested
