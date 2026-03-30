# Librova Feature Playbooks

This document captures the standard implementation paths for common vertical slices in Librova.

Use it to avoid forgetting layers, tests, logging, and document updates.

## 1. General Rule

Before starting a feature, identify which category it belongs to:

- read-side query
- mutation / use case
- transport contract change
- UI shell workflow
- stabilization / review

Do not start coding until the feature maps to one of the remaining roadmap buckets in `docs/Librova-Architecture-Master.md`.

## 2. Read-Side Query Playbook

Use for list, details, search, filters, and similar read-only catalog flows.

Checklist:

1. Add or extend the native application facade.
2. Keep domain and repository boundaries unchanged unless the use case truly needs a new contract.
3. Add or extend protobuf request/response messages.
4. Extend proto mapping.
5. Extend the protobuf service adapter.
6. Extend pipe transport method registration and request dispatch.
7. Extend the C# client.
8. Extend the C# service and UI-facing DTO mapping.
9. Extend the ViewModel and bind the UI.
10. Add tests at the touched boundaries.
11. Update `docs/ProjectDocumentation.md`.
12. Record the verified checkpoint in `docs/ImplementationProgress.md`.

Minimum verification:

- native facade test against the real repository path when practical
- protobuf mapping/adapter/dispatcher coverage
- C# client/service coverage
- UI ViewModel coverage
- one strong host-backed integration test when the query is user-facing and central

## 3. Mutation / Use Case Playbook

Use for import, export, delete-to-trash, settings apply, and similar state-changing features.

Checklist:

1. Write down the operational policy first.
2. Decide what is authoritative:
   database row, managed file, cover file, job record, or preference file.
3. Implement the native application use case.
4. Ensure rollback or failure semantics are explicit.
5. Add actionable logging on the mutation boundary.
6. Extend protobuf contracts.
7. Extend proto mapping.
8. Extend the protobuf service adapter.
9. Extend pipe transport method registration and dispatch.
10. Extend the C# client and UI service.
11. Extend the ViewModel and UI command wiring.
12. Add strong host-backed integration coverage.
13. Update stable docs and progress docs.

Mutation-specific questions to answer before coding:

- what happens on partial failure
- what gets rolled back
- what user-visible result means success
- whether the operation is idempotent
- whether the operation should be cancelable

## 4. Transport Contract Change Playbook

Use when adding or changing a protobuf message or pipe method.

Checklist:

1. Keep protobuf changes additive when possible.
2. Never reuse protobuf field numbers.
3. Never reuse named-pipe method ids.
4. Extend C++ and C# transport enums in the same checkpoint.
5. Extend whitelist/validation logic in `PipeProtocol` on both sides.
6. Run `scripts/ValidateProto.ps1`.
7. Rebuild generated/native transport code and rerun transport tests.

Required follow-through:

- `proto/import_jobs.proto`
- native `PipeProtocol`
- native request dispatcher
- native adapter
- C# `PipeProtocol`
- C# client and mapper
- relevant tests on both sides

## 5. UI Workflow Playbook

Use when wiring a new capability into the Avalonia shell.

Checklist:

1. Keep business decisions out of the view.
2. Expose a UI-facing service abstraction before the ViewModel if transport DTOs would leak upward.
3. Put user-visible state in the ViewModel.
4. Keep code-behind limited to view-specific desktop interaction.
5. Add validation before enabling destructive or state-changing commands.
6. Add logging for command start, success, and controlled failure.
7. Add ViewModel tests before adding UI polish.

Preferred test order:

1. mapper/service tests
2. ViewModel tests
3. shell composition tests
4. host-backed integration test if the workflow crosses the IPC boundary

## 6. Stabilization / Review Playbook

Use before or after a major phase boundary.

Checklist:

1. Review crash safety and shutdown behavior.
2. Review timeout and cancellation semantics.
3. Review logging coverage and noisy logs.
4. Review test realism and fake-test smell.
5. Review docs drift across architecture, project documentation, and progress.
6. Close the highest-risk findings before adding more features.

## 7. Done Criteria

A vertical slice is done only when all of the following are true:

- implementation is complete end-to-end for the intended MVP scope
- verification has been run sequentially
- logs exist on important boundaries
- the active roadmap no longer lists the slice as a remaining gap
- `docs/ProjectDocumentation.md` reflects the new stable reality
- `docs/ImplementationProgress.md` records the verified checkpoint
