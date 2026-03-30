# Codex Session Instructions for LibriFlow

This file is the startup checklist for each new coding session in this repository.

## 1. Read First

Before making changes, review these documents in this order:

1. `docs/LibriFlow-Architecture-Master.md`
2. `docs/CodeStyleGuidelines.md`
3. `docs/CommitMessageGuidelines.md`
4. `CodexSessionInstructions.md`

## 2. Non-Negotiable Project Constraints

- LibriFlow MVP targets Windows only.
- Architecture is two-process: `LibriFlow.UI` in C# / Avalonia and `LibriFlow.Core` in C++20.
- UI and core communicate through `gRPC + Protobuf`.
- One user, one managed library, offline-first.
- Unicode and Cyrillic correctness are mandatory.
- The backend must stay testable and isolated from UI concerns.

## 3. Working Rules for Each Task

- Start by checking the current repository state instead of assuming structure.
- Do not invent architecture that conflicts with frozen decisions in the master document.
- Keep domain logic out of Avalonia views and transport DTOs.
- Prefer small vertical slices that preserve clean boundaries.
- When adding new conventions, update the relevant document in the same task.

## 4. Implementation Priorities

When tradeoffs appear, prefer:

1. correctness
2. crash safety
3. explicit boundaries
4. testability
5. developer convenience

Do not trade away Unicode correctness, rollback safety, or contract clarity for speed.

## 5. Code Review Checklist

Before finishing a task, verify:

- naming matches `docs/CodeStyleGuidelines.md`;
- new code respects layer boundaries;
- file-system and Unicode handling are explicit;
- long-running work is non-blocking where required;
- tests cover the intended behavior or the gap is stated clearly;
- docs were updated if the change introduced a new rule or decision.

## 6. Commit Discipline

- Never commit unless explicitly requested.
- When a commit is requested, follow `docs/CommitMessageGuidelines.md`.
- Prefer one coherent commit per logical change.

## 7. Document Maintenance

If any of these files stop matching the actual repository, update them early:

- `docs/LibriFlow-Architecture-Master.md`
- `docs/CodeStyleGuidelines.md`
- `docs/CommitMessageGuidelines.md`
- `CodexSessionInstructions.md`
