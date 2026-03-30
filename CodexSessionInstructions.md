# Codex Session Instructions for Librova

This file is the startup checklist for each new coding session in this repository.

## 1. Read First

Before making changes, review these documents in this order:

1. `docs/Librova-Product.md`
2. `docs/Librova-Architecture.md`
3. `docs/Librova-Roadmap.md`
4. `docs/engineering/CodeStyleGuidelines.md`
5. `docs/engineering/CommitMessageGuidelines.md`
6. `docs/engineering/FeaturePlaybooks.md`
7. `docs/engineering/TestStrategy.md`
8. `docs/engineering/TransportInvariants.md`
9. `docs/archive/Development-History.md`
10. `CodexSessionInstructions.md`

Use `docs/archive/Architecture-History.md` only when the concise active documents are insufficient and historical planning detail is actually needed.

## 2. Non-Negotiable Project Constraints

- Librova MVP targets Windows only.
- Architecture is two-process: `Librova.UI` in C# / Avalonia and `Librova.Core` in C++20.
- UI and core communicate through `Protobuf` contracts over Windows named pipes for the MVP.
- One user, one managed library, offline-first.
- Unicode and Cyrillic correctness are mandatory.
- The backend must stay testable and isolated from UI concerns.

## 3. Working Rules for Each Task

- Start by checking the current repository state instead of assuming structure.
- Do not invent architecture that conflicts with frozen decisions in the active architecture document.
- Keep domain logic out of Avalonia views and transport DTOs.
- Prefer small vertical slices that preserve clean boundaries.
- When adding new conventions, update the relevant document in the same task.
- For native code, prefer one static library per logical slice under `libs/<SliceName>/`, with a local `CMakeLists.txt` in that folder.
- In native libraries, keep `.hpp` and `.cpp` files together in the same directory unless a different layout becomes clearly necessary.
- All build artifacts must go under the repository root `out/`; project-local `bin/` and `obj/` directories are not allowed as an intended steady state.
- Developer-facing run scripts and local runtime bootstrap flows should route disposable runtime files, logs, and transient state into `out/` whenever practical instead of scattering them across source directories or user profile folders.
- `CMake` is the canonical build system for native code; Visual Studio solutions exist for developer convenience, not as the primary source of build truth.
- If the SQLite schema depends on optional SQLite modules such as `FTS5`, declare the required features explicitly in `vcpkg.json` instead of relying on default port settings.
- External converter integration should stay user-configurable; `fb2cng` is the first built-in profile, not a hard-wired exclusive dependency.
- Conversion cancellation is a distinct outcome from converter failure; cancelled conversions must not silently fall back to storing the original FB2 file.
- Do not run `build` and `ctest` in parallel when tests depend on freshly built binaries; verification must run sequentially as `build -> test`.
- After changing files under `proto/`, validate contracts with `scripts/ValidateProto.ps1` before considering the checkpoint verified.
- Do not introduce `gRPC` runtime dependencies into the MVP path unless the architecture decision is explicitly revisited; the current baseline is transport-neutral protobuf adapters over named pipes.
- Process-level IPC tests must use explicit readiness checks and deterministic cleanup for spawned host processes instead of relying on fixed sleeps.
- Important execution paths in both C++ and C# must be covered by project logging; startup, shutdown, IPC boundaries, long-running jobs, and failure paths should emit actionable logs through the repository logging facade instead of ad hoc console output.
- Before starting a new feature, map it to one of the still-open roadmap buckets in `docs/Librova-Roadmap.md`.
- Do not start convenience or side-feature work unless it directly closes one of the remaining roadmap items or a concrete stabilization item.
- Finish the current product gap end-to-end before branching into adjacent polish or secondary UX improvements.
- If implemented reality removes a roadmap gap or changes product or architecture reality, update `docs/Librova-Product.md`, `docs/Librova-Architecture.md`, `docs/Librova-Roadmap.md`, and `docs/archive/Development-History.md` in the same task when relevant.
- If a UI workflow changes in a user-visible way, update `docs/ManualUiTestScenarios.md` in the same task so manual validation stays synchronized with the current app behavior.
- `docs/ManualUiTestScenarios.md` must be written in Russian, while all UI labels, button names, field names, and other on-screen strings must stay exactly as they appear in the English UI.
- For every new vertical slice, follow the appropriate checklist in `docs/engineering/FeaturePlaybooks.md` instead of inventing an ad hoc layer sequence.
- For every proposed new task, apply the scope in `docs/Librova-Product.md` and `docs/Librova-Roadmap.md` first; if the task does not close an active MVP bucket or stabilization item, do not start it.
- After any transport change, follow `docs/engineering/TransportInvariants.md` and verify both native and managed sides before treating the checkpoint as done.
- Use `docs/engineering/TestStrategy.md` to decide whether a unit, integration, or strong host-backed test is actually needed; do not add decorative tests.

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

- naming matches `docs/engineering/CodeStyleGuidelines.md`;
- new code respects layer boundaries;
- file-system and Unicode handling are explicit;
- long-running work is non-blocking where required;
- tests cover the intended behavior or the gap is stated clearly;
- docs were updated if the change introduced a new rule or decision;
- `docs/archive/Development-History.md` was updated when the task completed a verified checkpoint;
- the main human-readable project docs were updated when the task changed product, architecture, or roadmap reality;
- phase placement in `docs/archive/Development-History.md` still matches the current roadmap phase from `docs/Librova-Roadmap.md`;
- the vertical slice followed the relevant checklist in `docs/engineering/FeaturePlaybooks.md`;
- if the task changed transport, `docs/engineering/TransportInvariants.md` was respected and both language sides were verified.

## 6. Commit Discipline

- Never commit unless explicitly requested.
- When a commit is requested, follow `docs/engineering/CommitMessageGuidelines.md`.
- Prefer one coherent commit per logical change.

## 7. Document Maintenance

If any of these files stop matching the actual repository, update them early:

- `docs/Librova-Product.md`
- `docs/Librova-Architecture.md`
- `docs/Librova-Roadmap.md`
- `docs/ManualUiTestScenarios.md`
- `docs/engineering/CodeStyleGuidelines.md`
- `docs/engineering/CommitMessageGuidelines.md`
- `docs/engineering/FeaturePlaybooks.md`
- `docs/engineering/TestStrategy.md`
- `docs/engineering/TransportInvariants.md`
- `docs/engineering/ReviewChecklist.md`
- `docs/archive/Development-History.md`
- `docs/archive/Architecture-History.md`
- `CodexSessionInstructions.md`

If a new standing rule, workflow constraint, or repository convention appears during work, add it to this file in the same task instead of relying on memory.
If a step becomes a completed and verified checkpoint, record it in `docs/archive/Development-History.md` in the same task.
