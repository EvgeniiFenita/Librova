# Codex Session Instructions for Librova

This file is the startup checklist for each new coding session in this repository.

## 1. Read First

Before making changes, review these documents in this order:

1. `docs/Librova-Architecture-Master.md`
2. `docs/CodeStyleGuidelines.md`
3. `docs/CommitMessageGuidelines.md`
4. `docs/ProjectDocumentation.md`
5. `docs/ImplementationProgress.md`
6. `CodexSessionInstructions.md`

Use `docs/archive/Librova-Architecture-Full.md` only when the concise master document is insufficient and historical planning detail is actually needed.

## 2. Non-Negotiable Project Constraints

- Librova MVP targets Windows only.
- Architecture is two-process: `Librova.UI` in C# / Avalonia and `Librova.Core` in C++20.
- UI and core communicate through `Protobuf` contracts over Windows named pipes for the MVP.
- One user, one managed library, offline-first.
- Unicode and Cyrillic correctness are mandatory.
- The backend must stay testable and isolated from UI concerns.

## 3. Working Rules for Each Task

- Start by checking the current repository state instead of assuming structure.
- Do not invent architecture that conflicts with frozen decisions in the master document.
- Keep domain logic out of Avalonia views and transport DTOs.
- Prefer small vertical slices that preserve clean boundaries.
- When adding new conventions, update the relevant document in the same task.
- For native code, prefer one static library per logical slice under `libs/<SliceName>/`, with a local `CMakeLists.txt` in that folder.
- In native libraries, keep `.hpp` and `.cpp` files together in the same directory unless a different layout becomes clearly necessary.
- All build artifacts must go under the repository root `out/`; project-local `bin/` and `obj/` directories are not allowed as an intended steady state.
- Developer-facing run scripts and local runtime bootstrap flows should route disposable runtime files, logs, and transient state into `out/` whenever practical instead of scattering them across source directories or user profile folders.
- `CMake` is the canonical build system for native code; Visual Studio solutions exist for developer convenience, not as the primary source of build truth.
- If the SQLite schema depends on optional SQLite modules such as `FTS5`, declare the required features explicitly in `vcpkg.json` instead of relying on default port settings.
- Keep `docs/ProjectDocumentation.md` updated with stable implementation facts, not plans or aspirations.
- External converter integration should stay user-configurable; `fb2cng` is the first built-in profile, not a hard-wired exclusive dependency.
- Conversion cancellation is a distinct outcome from converter failure; cancelled conversions must not silently fall back to storing the original FB2 file.
- Do not run `build` and `ctest` in parallel when tests depend on freshly built binaries; verification must run sequentially as `build -> test`.
- After changing files under `proto/`, validate contracts with `scripts/ValidateProto.ps1` before considering the checkpoint verified.
- Do not introduce `gRPC` runtime dependencies into the MVP path unless the architecture decision is explicitly revisited; the current baseline is transport-neutral protobuf adapters over named pipes.
- Process-level IPC tests must use explicit readiness checks and deterministic cleanup for spawned host processes instead of relying on fixed sleeps.
- When a completed checkpoint belongs to a new roadmap phase, start that new phase explicitly in `docs/ImplementationProgress.md` instead of continuing to append entries under the previous phase.
- Important execution paths in both C++ and C# must be covered by project logging; startup, shutdown, IPC boundaries, long-running jobs, and failure paths should emit actionable logs through the repository logging facade instead of ad hoc console output.

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
- `docs/ImplementationProgress.md` was updated when the task completed a verified checkpoint.
- `docs/ProjectDocumentation.md` was updated when the task made current project reality more complete or more explicit.
- phase placement in `docs/ImplementationProgress.md` still matches the current roadmap phase from `docs/Librova-Architecture-Master.md`.

## 6. Commit Discipline

- Never commit unless explicitly requested.
- When a commit is requested, follow `docs/CommitMessageGuidelines.md`.
- Prefer one coherent commit per logical change.

## 7. Document Maintenance

If any of these files stop matching the actual repository, update them early:

- `docs/Librova-Architecture-Master.md`
- `docs/CodeStyleGuidelines.md`
- `docs/CommitMessageGuidelines.md`
- `docs/ImplementationProgress.md`
- `docs/ProjectDocumentation.md`
- `CodexSessionInstructions.md`
- `docs/archive/Librova-Architecture-Full.md`

If a new standing rule, workflow constraint, or repository convention appears during work, add it to this file in the same task instead of relying on memory.
If a step becomes a completed and verified checkpoint, record it in `docs/ImplementationProgress.md` in the same task.
